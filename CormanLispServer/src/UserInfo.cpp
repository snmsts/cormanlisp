//		File:		UserInfo.cpp
//		Contents:	User specific information class partial implementation.
//		History:	10/01/16  Artem Boldarev Created.
//					10/04/16  Artem Boldarev
//							  User profile directory retrieval.
//					10/05/16  Artem Boldarev
//							  Added length retrieving methods.
//					10/06/16  Artem Boldarev
//							  Personal (e.g. Documents) directory retrieval code.
//

#include "UserInfo.h"

#include <string.h>

#include "Stdafx.h"
#include "Os.h"

UserInfo::UserInfo(void)
: name(NULL), name_len(0),
profile_directory(NULL), profile_directory_len(0),
personal_directory(NULL), personal_directory_len(0),
version(31)
{
}

UserInfo::~UserInfo(void)
{
	if (name != NULL)
		delete[] name;

	if (profile_directory != NULL)
		delete[] profile_directory;

	if (personal_directory != NULL)
		delete[] personal_directory;
}

const char *UserInfo::GetName(void) const
{
	return name == NULL ? "" : name;
}

size_t UserInfo::GetNameLength(void) const
{
	return name_len;
}


const char *UserInfo::GetProfileDirectory(void) const
{
	return profile_directory == NULL ? "" : profile_directory;
}

size_t UserInfo::GetProfileDirectoryLength(void) const
{
	return profile_directory_len;
}

const char *UserInfo::GetPersonalDirectory(void) const
{
	return personal_directory == NULL ? "" : personal_directory;
}

size_t UserInfo::GetPersonalDirectoryLength(void) const
{
	return personal_directory_len;
}


int UserInfo::GetVersion(void) const
{
	return version;
}

//	Stores a copy of the text in what the destination points to.
static void keep(const char* text, char** destination, size_t* length)
{
	size_t n = strlen(text);
	char* copy = new char[n + 1];
	memcpy(copy, text, n + 1);
	*destination = copy;
	*length = n;
}

bool UserInfo::FillUserInfo(UserInfo &ui)
{
	char buffer[1024] = { 0 };
	bool user_name_status = osUserName(buffer, sizeof(buffer));
	if (user_name_status)
		keep(buffer, &ui.name, &ui.name_len);

	bool user_profile_status = osUserDirectory(buffer, sizeof(buffer));
	if (user_profile_status)
		keep(buffer, &ui.profile_directory, &ui.profile_directory_len);

	bool user_personal_status = osUserDocumentsDirectory(buffer, sizeof(buffer));
	if (user_personal_status)
		keep(buffer, &ui.personal_directory, &ui.personal_directory_len);

	return user_name_status && user_profile_status && user_personal_status;
}
