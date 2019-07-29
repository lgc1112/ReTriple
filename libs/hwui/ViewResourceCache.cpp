#include "ViewResourceCache.h"

namespace android {

#ifdef USE_OPENGL_RENDERER
using namespace uirenderer;
ANDROID_SINGLETON_STATIC_INSTANCE(ViewResourceCache);
#endif


namespace uirenderer {


ViewResourceCache::ViewResourceCache() : Singleton<ViewResourceCache>() {

}


ResourceData& ViewResourceCache::getResourceData(string id) {
	return *mResourceData[id];
}

void ViewResourceCache::setResourceData(string id, ResourceData* data) {
	mResourceData[id] = data;
}

void ViewResourceCache::generate(string id) {

	if (mResourceData[id] == NULL) {
		ResourceData* rData = new ResourceData();
		rData->hasReGenerated = true;
		mResourceData[id] = rData;
	}
	else {
		mResourceData[id]->hasReGenerated = true;
	}

}

void ViewResourceCache::draw(string id) {
	if (mResourceData[id] == NULL) {
		ResourceData* rData = new ResourceData();
		mResourceData[id] = rData;
		return;
	}
	if (mResourceData[id]->hasReGenerated) {
		mResourceData[id]->hasReGenerated = false;
		mResourceData[id]->reDrawCount = 0;
	}
	else {
		mResourceData[id]->reDrawCount++;
	}
}

string ViewResourceCache::print() {
	hash_map<string, ResourceData*> ::iterator it;
	it = mResourceData.begin();
	string s;
	while (it != mResourceData.end())
	{
		s += it->first + ":" + it->second->print();
		//cout << it->first << ":" << it->second->print() << endl;
		//it->second.print();
		++it;
	}
	return s;

}

}; // namespace uirenderer
}; // namespace android

