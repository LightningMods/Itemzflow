# ItemzFlow 

## Lead Developer (besides the obv)
- [Masterzorag](https://twitter.com/Masterzorag)

## Third-Party Libraries

- [fuse-nfs](https://github.com/sahlberg/fuse-nfs) 
- [libNFS](https://github.com/orbisdev/orbisdev-liborbisNfs)
- [libfuse](https://github.com/libfuse/libfuse)
- [FTPS4](https://github.com/xerpi/FTPS4) 
- [dr_mp3](https://github.com/mackron/dr_libs/blob/master/dr_mp3.h) 
- [liborbisAudio](https://github.com/orbisdev/orbisdev-liborbisAudio) 
- [Orbis SQLite](https://github.com/orbisdev/orbisdev-libSQLite) 
- [OrbisDev SDK](https://github.com/orbisdev/orbisdev) 
- [OOSDK for PRXs](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain) 
- [JSMN](https://github.com/zserge/jsmn) (MIT)
- [INI Parser](https://github.com/0xe1f/psplib/tree/master/libpsp) (Apache License 2.0)
- [RSA Verify (ARMmbed)](https://github.com/ARMmbed/mbedtls) (Apache License 2.0)
- [OpenSSL/MD5](https://github.com/openssl/openssl) (Apache License 2.0)
- [BusyBox](https://elixir.bootlin.com/busybox/0.39/source) (GPLv2)
- [LibPNG](https://github.com/glennrp/libpng) 
- [log.c](https://github.com/rxi/log.c)
- [Game Dumper](https://github.com/Al-Azif/dumper-testing)
- [libjbc](https://github.com/sleirsgoevy/ps4-libjbc)
- [pugixml](https://github.com/zeux/pugixml)
- [taglib](https://github.com/taglib/taglib)
- Libz

## Refs
- [ShaderToy](shadertoy.com) 

# Settings

the ini file is either loaded by the app dir or from USB0 when the app is booted

#### Settings Layout 

| INI Key             | Description                                                 |
|---------------------|-------------------------------------------------------------|
| `Dumper_option`     | 0 = ALL  1 = BASE Game only 2 = Patch only                  |
| `Sort_By`           | -1 is NA 0 = Alpha order by TID 1 = Alpha order by App Name |
| `Sort_Cat`          | Sort category                                               |
| `cover_message`     | Shows cover message on app startup                          |
| `MP3_Path`          | FS Path to a folder of MP3s or a single MP3 to play on loop |
| `Dumper_Path`       | Dump path                                                   |
| `TTF_Font`          | TTF Font the store will try to use (embedded font on fail)  |
| `Show_Buttons`      | Shows IF buttons on the screen                              |
| `Enable_Theme`      | ONLY Active if you have a theme enabled                     |
| `Image_path`        | background PNG image                                        |
| `Reflections`       | Enables cover reflections in IF                             |
| `Home_Redirection`  | Enables the Home Menu Redirect                              |
| `Daemon_on_start`   | Disables the Daemon from auto connecting with the app       |
| `Image_path`        | Background Image                                            |
| `Show_install_prog` | Enables the Store PKG/APP install Progress                  |
| `Fuse_IP`           | PC NFS Share in format `nfs://IP/share_name`                |

#### setting.ini example

```ini
[Settings]
Dumper_option=0 
Sort_By=-1
Sort_Cat=0 
cover_message=1 
MP3_Path=/mnt/usb0/music 
Dumper_Path=/mnt/usb0 
TTF_Font=/mnt/usb0/myfont.ttf
Show_Buttons=1 
Enable_Theme=1 
Image_path=/mnt/usb0/pic.png 
Reflections=1 
Home_Redirection=1 
Daemon_on_start=0 
Show_install_prog=1 
Fuse_IP=nfs://192.1.2.3/nfs_share
```

## NFS Share format

the NFS formatted string can be entered in the app using the in-app keyboard or set in the settings.ini file

```
nfs://PC_IP/share_name
```
or if your share name is /
```
PC_IP
```

## Itemzflow Log
- If the app crashes or mentions "a FATAL Signal" send this log to us via the discord below or via GH Issues
`/user/app/ITEM00001/logs/itemzflow_app.log`

## Daemon

The itemz daemon is installed when you open the app for the first time at `/system/vsh/app/ITEM00002`
it gets updated ONLY by Itemzflow

The daemon folder is ONLY for internal use by the Itemzflow devs
it is hash checked by the loader to ensure the daemon is always up to date with the PKG

## Dumper details

- Dumper log is at `/user/app/ITEM00001/logs/if_dumper.log`
- Dumper WILL ONLY dump to the first USB it finds (most likely USB0)
- Dumper will try to use the same Language as Itemzflow
- If the dumper gets stuck on a file make SURE **both** discs and **ALL** Languages are installed if available **BEFORE** Dumping
- If your dump has failed then provide us the log `/user/app/ITEM00001/logs/if_dumper.log` in our discord below

## Languages

- The Itemzflow's Langs. repo is [HERE](https://github.com/LightningMods/Itemzflow-Languages)
- The Itemzflow uses the PS4's System software Lang setting

## Themes

- You can download custom themes released in the [Itemzflow Themes Repo](https://github.com/LightningMods/itemzflow-themes)
- You can also make your own custom themes by following these instructions.

#### IMPORTANT the Theme files must HAVE the exact filename as listed below  

| Filename          | Description                |
|-------------------|----------------------------|
| `btn_o.png`       | O Button (67x68)           |
| `btn_x.png`       | X Button (67x68)           |
| `btn_tri.png`     | Triangle Button (67x68)    |
| `btn_sq.png`      | square Button (67x68)      |
| `btn_r1.png`      | R1 Button (309x152)        |
| `btn_l1.png`      | L1 Button (120x59)         |
| `btn_l2.png`      | L2 Button (120x105)        |
| `btn_options.png` | Options Button (145x84)    |
| `btn_up.png`      | D-Pad Up Button (32x32)    |
| `btn_down.png`    | D-Pad Down Button (32x32)  |
| `btn_left.png`    | D-Pad Left Button (32x32)  |
| `btn_right.png`   | D-Pad Right Button (32x32) |
| `font.ttf`        | Theme Font                 |
| `background.png`  | Background Image           |
| `shader.bin`      | PS4 GLES Compiled Shader   |
| `theme.ini`       | Theme Info                 |

#### Theme INI Config 

| INI Key | Description                                      |
|---------|--------------------------------------------------|
| Name    | Theme Name                                       |
| Author  | Who Made it                                      |
| Date    | Date it was made on                              |
| Version | Theme Version Number                             |
| Image   | 1 if your Theme has a background image, 0 if not |
| Font    | 1 = Theme has font.ttf 0 = It doesnt             |
| Shader  | 1 = Has Shader bin 0 = It doesnt                 |

#### Example Theme config

```ini
[THEME]
Name=Example Theme
Author=Example Author
Date=10/16/2022
Version=1.00
Image=1
Font=1
Shader=0

```

## App logs and their paths

| Service            | PS4 Path                                     |
|--------------------|----------------------------------------------|
| Itemzflow main app | `/user/app/ITEM00001/logs/itemzflow_app.log` |
| Dumper             | `/user/app/ITEM00001/logs/if_dumper.log`     |
| Itemz Loader       | `/user/app/ITEM00001/logs/loader.log`        |
| Itemz Daemon       | `/data/itemzflow_daemon/daemon.log`          |
| libfuse            | `/data/itemzflow_daemon/libfuse.log`         |

#### On USB (only on failure or crash) 

| Service            | USB Path                                    |
|--------------------|---------------------------------------------|
| Itemzflow main app | `/mnt/usb<USB_NUMBER>/itemzflow/crash.log`  |
| Dumper             | `/mnt/usb<USB_NUMBER>/itemzflow/dumper.log` |
| Itemz Loader       | `/mnt/usb<USB_NUMBER>/itemzflow/loader.log` |
| Itemz Daemon       | `/mnt/usb<USB_NUMBER>/itemzflow/daemon.log` |

## Official Discord server

Invite: https://discord.gg/GvzDdx9GTc

## Game Patches
Custom Game Patches for PlayStation 4 Games.

### Features
* `.xml` support

### Usage

### Easy Installation
- Patches can be configured, install/update via:
  - [GoldHEN Cheat Manager](https://github.com/GoldHEN/GoldHEN_Cheat_Manager/releases/latest)
  - [Itemzflow Game Manager](https://github.com/LightningMods/Itemzflow)
- Run your game.

### Storage
* Use `FTP` to upload patch files to:
  * `/user/data/GoldHEN/patches/xml/`
* Naming conversion for app and patch engine to recognize: `(TitleID).{format}`
  * e.g. `CUSA00001.xml`
  * e.g. `CUSA03694.xml`

### Patch types

| `type`    | Info                      | Value (example)        |
|-----------|---------------------------|------------------------|
| `byte`    | Hex, 1 byte               | `"0x00"`               |
| `bytes16` | Hex, 2 bytes              | `"0x0000"`             |
| `bytes32` | Hex, 4 bytes              | `"0x00000000"`         |
| `bytes64` | Hex, 8 bytes              | `"0x0000000000000000"` |
| `bytes`   | Hex, any size (no spaces) | `"####"`               |
| `float32` | Float, single             | `"1.0"`                |
| `float64` | Float, double             | `"1.0"`                |
| `utf8`    | String, UTF-8*            | `"string"`             |
| `utf16`   | String, UTF-16*           | `"string"`             |

#### Example patch

```xml
<?xml version="1.0" encoding="utf-8"?>
<Patch>
    <!--
      This will not be used by the plugin parser.
      It is only used for generating the files for distribution.
    -->
    <TitleID>
        <ID>EXAMPLE01</ID>
        <ID>EXAMPLE02</ID>
    </TitleID>
    <Metadata Title="Example Game Title"
              Name="Example Name"
              Note="Example Note"
              Author="Example Author"
              PatchVer="1.0"
              AppVer="00.34"
              AppElf="eboot.bin">
        <PatchList>
            <!-- This is a code comment, improves code readability. -->
            <!-- Code comment at end of line is also supported. -->
            <Line Type="bytes" Address="0x00000000" Value="0102030405060708"/>
            <Line Type="utf8" Address="0x00000000" Value="Hello World\x00"/>
        </PatchList>
    </Metadata>
</Patch>
```

## Donations

We accept the following methods

- [Ko-fi](https://ko-fi.com/lightningmods)
- BTC: bc1qgclk220glhffjkgraju7d8xjlf7teks3cnwuu9

if you donate and dont want to the message anymore created this folder after donating ``

## Credits

- [Flatz](https://twitter.com/flat_z)
- [SocraticBliss](https://twitter.com/SocraticBliss)
- [TheoryWrong](https://twitter.com/TheoryWrong)
- Znullptr
- [Xerpi](https://twitter.com/xerpi)
- [TOXXIC407](https://twitter.com/TOXXIC_407)
- [Masterzorag](https://twitter.com/masterzorag)
- [Psxdev/BigBoss](https://twitter.com/psxdev)
- [Specter](https://twitter.com/SpecterDev)
- [SocraticBliss](https://twitter.com/SocraticBliss)
- [Sleirsgoevy](https://github.com/sleirsgoevy/)
- [AlAzif](https://github.com/al-azif)
- BigBoss
- [bucanero](https://github.com/bucanero) (Game Saves)
- [illusion](https://github.com/illusion0001) (Game Patches/Trainers)
- [Pharaoh2k](https://github.com/Pharaoh2k) (Apps/Games/DLCs Databases Recovery)
