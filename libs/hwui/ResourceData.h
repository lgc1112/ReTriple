#ifndef RESOURCE_DATA_H
#define RESOURCE_DATA_H
#include <string>
#include <stdio.h>
using namespace std;
#define NUM 12
class ResourceData
{
public:
	bool hasReGenerated ;
	int reDrawCount;
	int reDrawCounts[NUM];
	int aveReDrawCount;
	ResourceData(void){
		for(int i = 0; i < NUM; i++)
			reDrawCounts[i] = -1;
		aveReDrawCount = -1;
	}
	void updateReDrawCounts(int redrawC){
		int tmp = 0;
		for(int i = 0; i <= NUM - 2; i++){
			reDrawCounts[i] = reDrawCounts[i + 1];
			tmp += reDrawCounts[i];
			
		}
		reDrawCounts[NUM - 1] = redrawC;
		tmp += reDrawCounts[NUM - 1];
		if(reDrawCounts[0] != -1)
			aveReDrawCount = tmp / NUM;
	}
	
	string print(void) {
		char str[128];
		
		sprintf(str, "%d", reDrawCount);
		string s = (string)"reDrawCount: " +  str + "\n";
			return s;
	}

};

#endif 
