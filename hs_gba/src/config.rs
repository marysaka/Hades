use std::fmt;

use crate::libgba_sys;

#[allow(non_camel_case_types)]
#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd, Hash, Debug)]
pub enum BackupStorageType {
    EEPROM_4K = 1,
    EEPROM_64K = 2,
    SRAM = 3,
    Flash64 = 4,
    Flash128 = 5,
}

impl fmt::Display for BackupStorageType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Self::EEPROM_4K => write!(f, "EEPROM_4K"),
            Self::EEPROM_64K => write!(f, "EEPROM_64K"),
            Self::SRAM => write!(f, "SRAM"),
            Self::Flash64 => write!(f, "Flash64"),
            Self::Flash128 => write!(f, "Flash128"),
        }
    }
}

impl BackupStorageType {
    pub(crate) fn raw(&self) -> libgba_sys::backup_storage_types {
        match self {
            Self::EEPROM_4K => libgba_sys::backup_storage_types::BACKUP_EEPROM_4K,
            Self::EEPROM_64K => libgba_sys::backup_storage_types::BACKUP_EEPROM_64K,
            Self::SRAM => libgba_sys::backup_storage_types::BACKUP_SRAM,
            Self::Flash64 => libgba_sys::backup_storage_types::BACKUP_FLASH64,
            Self::Flash128 => libgba_sys::backup_storage_types::BACKUP_FLASH128,
        }
    }
}

pub struct GbaConfig {
    rom: Vec<u8>,
    bios: Vec<u8>,
    skip_bios: bool,
    audio_frequency: u32,
    rtc: bool,
    backup_storage_type: Option<BackupStorageType>,
    backup_storage_data: Vec<u8>,
}

impl GbaConfig {
    pub fn new() -> Self {
        GbaConfig {
            rom: Vec::new(),
            bios: Vec::new(),
            skip_bios: false,
            audio_frequency: 0,
            rtc: false,
            backup_storage_type: None,
            backup_storage_data: Vec::new(),
        }
    }

    pub fn rom(&self) -> &[u8] {
        &self.rom
    }

    pub fn with_rom(&mut self, rom: Vec<u8>) {
        self.rom = rom
    }

    pub fn bios(&self) -> &[u8] {
        &self.bios
    }

    pub fn with_bios(&mut self, bios: Vec<u8>) {
        self.bios = bios
    }

    pub fn skip_bios(&mut self, skip: bool) {
        self.skip_bios = skip;
    }

    pub fn with_audio_frequency(&mut self, audio_frequency: u32) {
        self.audio_frequency = audio_frequency
    }

    pub fn rtc(&self) -> bool {
        self.rtc
    }

    pub fn with_rtc(&mut self, rtc: bool) {
        self.rtc = rtc;
    }

    pub fn backup_storage_type(&self) -> Option<BackupStorageType> {
        self.backup_storage_type
    }

    pub fn with_backup_storage_type(&mut self, backup_storage_type: BackupStorageType) {
        assert!(self.backup_storage_data.len() == 0);
        self.backup_storage_type = Some(backup_storage_type);
    }

    pub fn with_backup_storage_data(&mut self, data: Vec<u8>) {
        assert!(self.backup_storage_type.is_some());
        self.backup_storage_data = data;
    }

    pub fn without_backup_storage(&mut self) {
        self.backup_storage_type = None;
        self.backup_storage_data = Vec::new();
    }

    pub fn detect_backup_storage(&mut self) {
        // Auto-detection algorithm is very simple: we look for a bunch of strings in the game's ROM.

        let rom_search = |needle: &[u8]| {
            self.rom()
                .windows(needle.len())
                .position(|w| w == needle)
                .is_some()
        };

        if rom_search("EEPROM_V".as_bytes()) {
            self.backup_storage_type = Some(BackupStorageType::EEPROM_64K);
        } else if rom_search("SRAM_V".as_bytes()) || rom_search("SRAM_F_V".as_bytes()) {
            self.backup_storage_type = Some(BackupStorageType::SRAM);
        } else if rom_search("FLASH1M_V".as_bytes()) {
            self.backup_storage_type = Some(BackupStorageType::Flash128);
        } else if rom_search("FLASH_V".as_bytes()) || rom_search("FLASH512_V".as_bytes()) {
            self.backup_storage_type = Some(BackupStorageType::Flash64);
        } else {
            self.backup_storage_type = None;
        }
    }
}

impl GbaConfig {
    pub(crate) fn raw(&self) -> libgba_sys::gba_config {
        libgba_sys::gba_config {
            rom: self.rom.as_ptr(),
            rom_size: self.rom.len(),
            bios: self.bios.as_ptr(),
            bios_size: self.bios.len(),
            skip_bios: self.skip_bios,
            audio_frequency: self.audio_frequency,
            rtc: self.rtc,
            backup_storage_type: self
                .backup_storage_type
                .as_ref()
                .and_then(|t| Some(t.raw()))
                .unwrap_or(libgba_sys::backup_storage_types::BACKUP_NONE),
            backup: self.backup_storage_data.as_ptr(),
            backup_size: self.backup_storage_data.len(),
        }
    }
}
