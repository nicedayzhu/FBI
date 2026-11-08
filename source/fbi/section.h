#pragma once

void mainmenu_open();
void language_open();

void dumpnand_open();
void extsavedata_open();
void files_open(FS_ArchiveID archiveId, FS_Path archivePath);
void files_open_sd();
void files_open_ctr_nand();
void files_open_twl_nand();
void files_open_twl_photo();
void files_open_twl_sound();
void pendingtitles_open();
void remoteinstall_open();
void patchinstall_open();
void* patchinstall_listener_open();
bool patchinstall_listener_poll(void* listener);
void patchinstall_listener_prompt(void* listener);
void patchinstall_listener_close(void* listener);
void systemsavedata_open();
void tickets_open();
void titles_open();
void update_open();
