/*
 *  Resource Handler: functions for loading all supported data types
 *      - Loads JSON scene information from jsoncpp (https://github.com/open-source-parsers/jsoncpp)
 *      - Loads TGA textures via FreeImage
 *      - Loads 3Ds models via Assimp
 */

#include <Resources/resourcehandler.hh>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <FreeImage.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <memory>
#include <vector>
#include <fstream>

#include <Utilities/exceptions.hh>
#include <Utilities/log.hh>
#include <Resources/texture.hh>

using json = nlohmann::json;

void ResourceHandler::loadResources(const json &scene) {
    // TODO Implement texture loading
    // NOTE What other types of resources might i want to load on scene start
    if(scene.find("resources") != scene.end()) {
        std::vector<json> resources = scene["resources"];
        for(const json &resource : resources) {
            try {
                std::string type = resource["type"];

                if(type == "model3d") {
                    loadModel3D(resource["resourcename"], resource["filename"]);
                } else if(type == "shader") {
                    loadShader(resource);
                }
            } catch(std::domain_error &error) {
                Log::getErrorLog<SyncLogger>() << error.what() << '\n';
            }
        }
    }
}

const std::unordered_map<std::string, Shader> &ResourceHandler::getShaders() const {
    return m_shaders;
}

Shader &ResourceHandler::getShader(const std::string &shader) {
    auto ret = m_shaders.find(shader);
    if(ret != m_shaders.end()) {
        return ret->second;
    }
    throw BadResource("Couldn't find shader", shader);
}

void ResourceHandler::loadShader(const json &object) {
    std::string name = object["resourcename"];
    if(m_shaders.find(name) != m_shaders.end()) {
        return;
    }

    std::string filename = object["filename"];
    std::vector<std::string> files = object["types"];
    if(files.size() == 0) {
        throw BadResource("types is empty", name);
    }
    std::vector<GLenum> types;
    for(auto &file : files) {
        if(file == "fragment") {
            types.push_back(GL_FRAGMENT_SHADER);
            file = filename + '/' + filename + "_fs.glsl";
        } else if(file == "vertex") {
            types.push_back(GL_VERTEX_SHADER);
            file = filename + '/' + filename + "_vs.glsl";
        } else if(file == "geometry") {
            types.push_back(GL_GEOMETRY_SHADER);
            file = filename + '/' + filename + "_gs.glsl";
        }
    }

    Shader shader(files, types);

    if(object.find("uniforms") != object.end()) {
        std::vector<std::string> uniforms = object["uniforms"];
        for(const auto &uniform : uniforms) {
            shader.registerUniform(uniform);
        }
    }

    m_shaders.emplace(name, std::move(shader));
}

Texture &ResourceHandler::getTexture(const std::string &name) {
    auto res = m_textures.find(name);
    if(res != m_textures.end()) {
        return res->second;
    } else {
        throw BadResource("Texture not found", name);
    }
}

void ResourceHandler::loadTexture(const std::string &resourcename, const std::string &name, bool genMipMaps) {
    if(m_textures.find(resourcename) != m_textures.end()) {
        return;
    }

    FIBITMAP *img;
    auto filename = "Resources/Textures/" + name + ".tga";
    FREE_IMAGE_FORMAT format = FreeImage_GetFIFFromFilename(filename.c_str());

    if(!FreeImage_FIFSupportsReading(format)) {
        throw BadResource("FreeImage can't read from file", filename);
    }

    if(format == FIF_UNKNOWN) {
        throw BadResource("Unknown format", filename);
    }

    img = FreeImage_Load(format, filename.c_str());

    if(!img) {
        throw BadResource("Couldn't load image data", filename);
    }

    if(FreeImage_GetBPP(img) != 32) {
        FIBITMAP* oldImg = img;
        img = FreeImage_ConvertTo32Bits(oldImg);
        FreeImage_Unload(oldImg);
    }

    int height, width;
    width = FreeImage_GetWidth(img);
    height = FreeImage_GetHeight(img);

    unsigned char* bytes = FreeImage_GetBits(img);

    if(bytes == nullptr) {
        FreeImage_Unload(img);
        throw BadResource("couldn't load image bytes", filename);
    }

    GLuint glTexture;
    glBindTexture(GL_TEXTURE_2D, 0);
    glGenTextures(1, &glTexture);
    glBindTexture(GL_TEXTURE_2D, glTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, bytes);

    if(genMipMaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    if(!glIsTexture(glTexture)) {
        FreeImage_Unload(img);
        throw BadResource("texture is not valid", filename);
    }

    FreeImage_Unload(img);

    m_textures.emplace(resourcename, Texture(glTexture, resourcename));
    // m_textures.insert(std::make_pair(resourcename, Texture(glTexture, resourcename)));
}

Model3D &ResourceHandler::getModel3D(const std::string &name) {
    auto res = m_model3Ds.find(name);
    if(res != m_model3Ds.end()) {
        return res->second;
    } else {
        throw BadResource("Model3D not found", name);
    }
}

void ResourceHandler::loadModel3D(const std::string &resourcename, const std::string &modelname) {
    if(m_model3Ds.find(resourcename) != m_model3Ds.end()) {
        return;
    }

    std::string filename = "Resources/Models/" + modelname + ".3ds";
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filename.c_str(),
        aiProcess_CalcTangentSpace |
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_SortByPType );

    if(!scene || !scene->mRootNode || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE) {
        throw BadResource(importer.GetErrorString(), filename);
    }

    std::unordered_map<std::string, Texture*> textures;
    aiMaterial** materials = scene->mMaterials;
    for(unsigned int i = 0; i < scene->mNumMaterials; i++) {
        aiString aName;
        materials[i]->Get(AI_MATKEY_NAME, aName);
        std::string texname = std::string(aName.C_Str());
        std::string texresname = "tex_" + texname;
        std::string texfilename = modelname + "/" + texname;
        try {
            loadTexture(texresname, texfilename, true);
            textures[texresname] = &(getTexture(texresname));
        } catch(BadResource &error) {
            error.printError();
        }
    }

    std::vector<Model3D::Mesh> meshes;
    loadNode(scene, scene->mRootNode, glm::mat4(1.0f), meshes);

    for(auto &mesh : meshes) {
        aiString aName;
        unsigned int index = mesh.getMatIndex();
        if(index < scene->mNumMaterials) {
            materials[mesh.getMatIndex()]->Get(AI_MATKEY_NAME, aName);
            std::string resname = std::string("tex_") + aName.C_Str();
            mesh.setMatName(resname);
        }
    }

    m_model3Ds.emplace(resourcename, Model3D(std::move(meshes), std::move(textures)));
}

void ResourceHandler::copyaiMat(const aiMatrix4x4* from, glm::mat4 &to) {
    to[0][0] = from->a1; to[1][0] = from->a2;
    to[2][0] = from->a3; to[3][0] = from->a4;
    to[0][1] = from->b1; to[1][1] = from->b2;
    to[2][1] = from->b3; to[3][1] = from->b4;
    to[0][2] = from->c1; to[1][2] = from->c2;
    to[2][2] = from->c3; to[3][2] = from->c4;
    to[0][3] = from->d1; to[1][3] = from->d2;
    to[2][3] = from->d3; to[3][3] = from->d4;
}

void ResourceHandler::loadNode(const aiScene* scene, const aiNode* node, const glm::mat4 &parentTransform, std::vector<Model3D::Mesh> &meshes) {
    glm::mat4x4 transformation;
    copyaiMat(&node->mTransformation, transformation);
    transformation = parentTransform * transformation;

    for(unsigned int i = 0; i < node->mNumChildren; i++) {
        loadNode(scene, node->mChildren[i], transformation, meshes);
    }

    for(unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        std::vector<Model3D::Vertex> vertices;
        std::vector<unsigned int> indices;

        for(unsigned int j = 0; j < mesh->mNumVertices; j++) {
            Model3D::Vertex vertex;

            glm::vec4 position = transformation * glm::vec4(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z, 1.0f);
            vertex.position = glm::vec3(position.x, position.y, position.z);

            glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transformation)));
            vertex.normal = normalMatrix * (mesh->HasNormals()
                ? glm::vec3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z)
                : glm::vec3(0.0f, 0.0f, 0.0f));

            vertex.texCoord = (mesh->HasTextureCoords(0))
                ? glm::vec2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y)
                : glm::vec2(0.0f, 0.0f);

            vertices.push_back(vertex);
        }

        if(mesh->HasFaces()) {
            for(unsigned int j = 0; j < mesh->mNumFaces; j++) {
                aiFace face = mesh->mFaces[j];
                for(unsigned int k = 0; k < face.mNumIndices; k++) {
                    indices.push_back(face.mIndices[k]);
                }
            }
        } else {
            throw BadResource("Node was missing faces, load canceled");
        }

        meshes.emplace_back(std::move(vertices), std::move(indices));
        meshes.back().setMatIndex(mesh->mMaterialIndex);
    }
}
