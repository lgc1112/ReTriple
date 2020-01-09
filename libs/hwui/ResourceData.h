#ifndef RESOURCE_DATA_H
#define RESOURCE_DATA_H
#include <string>
#include <stdio.h>
using namespace std;
class ResourceData
{
public:
	bool hasReGenerated ;
	int reDrawCount;
	int reDrawCounts[3];
	int aveReDrawCount;
	ResourceData(void){
		reDrawCounts[0] = -1;
		reDrawCounts[1] = -1;
		reDrawCounts[2] = -1;
		aveReDrawCount = -1;
	}
	void updateReDrawCounts(int redrawC){
		reDrawCounts[0] = reDrawCounts[1];
		reDrawCounts[1] = reDrawCounts[2];
		reDrawCounts[2] = redrawC;
		if(reDrawCounts[0] != -1)
			aveReDrawCount = (reDrawCounts[0] + reDrawCounts[1] + reDrawCounts[2]) / 3;
	}
	
	string print(void) {
		char str[128];
		
		sprintf(str, "%d", reDrawCount);
		string s = (string)"reDrawCount: " +  str + "\n";
			return s;
	}

};

#endif 
