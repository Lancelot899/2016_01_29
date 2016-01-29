#include "user.h"
#include <stdio.h>
using namespace lancelot;

User::User(char* ID) {
	if (ID != NULL && strlen(ID) < 16) {
		int len = strlen(ID);
		memcpy(_ID, ID, len);
		_ID[len++] = '.';
		_ID[len++] = 'd';
		_ID[len++] = 'b';
		_ID[len] = 0;
		while (SQLITE_ERROR == (sqlite3_open(_ID, &_db))) {
			printf("open sqlite db error!\n");
			Sleep(1000);
		}
		
	}
}

User::~User(){

}

