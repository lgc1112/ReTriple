#ifndef VIEW_RESOURCE_CACHE_H
#define VIEW_RESOURCE_CACHE_H
#include <hash_map>
#include <string>
#include <utils/Singleton.h>

#include "ResourceData.h"


namespace android {
namespace uirenderer {


class  ViewResourceCache : public Singleton<ViewResourceCache> {
	ViewResourceCache();

	friend class Singleton<ViewResourceCache>;
private:	
	hash_map<string, ResourceData*> mResourceData;


public:
//	ResourceData& getResourceData(string id);
//	void setResourceData(string id, ResourceData* data);
	void generate(string id);
	void draw(string id);
	int getRedrawCount(string id);  
	string print(void);


};
}; // namespace uirenderer
};



#endif 


