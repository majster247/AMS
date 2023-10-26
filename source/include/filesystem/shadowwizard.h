#ifndef __MYOS__FILESYS__SHADOWWIZARD_H
#define __MYOS__FILESYS__SHADOWWIZARD_H

#include <common/types.h>
#include <drivers/ata.h>


namespace myos {

	namespace filesystem {


		bool FileIf(common::uint32_t sector);

		common::uint32_t fnv1a(char*);
		

		common::uint32_t AddTable(char* name);
		common::uint32_t RemoveTable(char* name);

		bool NewFile(char* name, common::uint8_t* file, 
					 common::uint32_t size);


		bool WriteFile(char* name, common::uint8_t* file, 
					   common::uint32_t size);
		bool WriteLBA(char* name, common::uint8_t* file, 
					  common::uint32_t lba);
		

		bool ReadFile(char* name, common::uint8_t* file,	
					  common::uint32_t size);
		bool ReadLBA(char* name, common::uint8_t* file,	
					  common::uint32_t lba);
		

		bool DeleteFile(char* name);

		common::uint32_t GetFileSize(char* name);
		//common::uint32_t FileList();
	}
}



#endif