#ifndef _RESOURCE_H
#define _RESOURCE_H

#include <string>

class Resource {
public:
    Resource();
    virtual ~Resource();

    std::string GetType();

private:
    std::string m_type;
};

#endif // _RESOURCE_H
