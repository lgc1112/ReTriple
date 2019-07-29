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
	string print(void) {
		char str[128];
		
		sprintf(str, "%d", reDrawCount);
		string s = (string)"reDrawCount: " +  str + "\n";
			return s;
	}

};

#endif 
