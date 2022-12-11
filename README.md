# ItemzFlow 

## Lead Developer (besides the obv)
- [Masterzorag](https://twitter.com/Masterzorag)

## Third-Party Libraries

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
- [yaml-cpp](https://github.com/jbeder/yaml-cpp)
- [taglib](https://github.com/taglib/taglib)
- Libz

## Refs
- [ShaderToy](shadertoy.com) 

# Settings

the ini file is either loaded by the app dir or from USB0 when the app is booted

#### Settings Layout 


| INI Key      | Description |
| ------------- | ------------- |
| Dumper_option     |  0 = ALL  1 = BASE Game only 2 = Patch only  |
| Sort_By     | -1 is NA 0 = Alpha order by TID 1 = Alpha order by App Name  |
| Sort_Cat   | Sort category  |
| cover_message     | Shows cover message on app startup  |
| MP3_Path     | FS Path to a folder of MP3s or a single MP3 to play on loop  |
| Dumper_Path    | Dump path  |
| TTF_Font  | TTF Font the store will try to use (embedded font on fail)  |
| Show_Buttons  | Shows IF buttons on the screen  |
| Enable_Theme     | ONLY Active if you have a theme enabled  |
| Image_path  | background PNG image  |
| Reflections     | Enables cover reflections in IF  |
| Home_Redirection  | Enables the Home Menu Redirect  |
| Daemon_on_start  | Disables the Daemon from auto connecting with the app  |
| Image_path  | Background Image  |
| Show_install_prog  | Enables the Store PKG/APP install Progress  |


#### setting.ini example 

```
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
```

## Itemzflow Log
- If the app crashes or mentions "a FATAL Signal" send this log to us via the discord below or via GH Issues
`/user/app/ITEM00001/logs/itemzflow_app.log`


## Daemon

The itemz daemon is installed when you open the app for the first time at `/system/vsh/app/ITEM00002`
it gets Updated ONLY by the Itemzflwo

The daemon settings file is ONLY for internal use by the Store devs
however it also has a ini at `/system/vsh/app/ITEM00002/daemon.ini` with the following ini values

```
[Daemon]
version=0x1001// Daemon version for Store, Official version is always > 0x1000
```

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

- You can download custom themes released in the [Itemzflow Themes Repo](https://github.com/LightningMods/xxxxxxxx)
- You can also make your own custom themes by following these instructions.

#### IMPORTENT the Theme files HAVE to have the exact filename as listed below  

| Filename      | Description |
| ------------- | ------------- |
| btn_o.png     | O Button (67x68)  |
| btn_x.png     | X Button (67x68)  |
| btn_tri.png   | Triangle Button (67x68)  |
| btn_sq.png     | square Button (67x68)  |
| btn_r1.png     | R1 Button (309x152)  |
| btn_l1.png     | L1 Button (120x59)  |
| btn_l2.png  | L2 Button (120x105)  |
| btn_options.png  | Options Button (145x84)  |
| btn_up.png     | D-Pad Up Button (32x32)  |
| btn_down.png  | D-Pad Down Button (32x32)  |
| btn_left.png     | D-Pad Left Button (32x32)  |
| btn_right.png  | D-Pad Right Button (32x32)  |
| font.ttf  | Theme Font  |
| background.png  | Background Image  |
| shader.bin  | PS4 GLES Compiled Shader  |
| theme.ini | Theme Info  |

#### Theme INI Config 


| INI Key      | Description |
| ------------- | ------------- |
| Name     | Theme Name  |
| Author     | Who Made it  |
| Date   | Date it was made on  |
| Version     | Theme Version Number |
| Image     | 1 if your Theme has a background image, 0 if not  |
| Font     | 1 = Theme has font.ttf 0 = It doesnt |
| Shader     | 1 = Has Shader bin 0 = It doesnt  |


#### Example Theme config 
```
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

| Service      | PS4 Path |
| ------------- | ------------- |
| Itemzflow main app     | /user/app/ITEM00001/logs/itemzflow_app.log  |
| Dumper    | /user/app/ITEM00001/logs/if_dumper.log  |
| Itemz Loader   | /user/app/ITEM00001/logs/loader.log  |
| Itemz Daemon   | /data/itemzflow_daemon/daemon.log |
| libfuse     | /user/app/ITEM00001/logs/libfuse.log  |

#### On USB (only on failure or crash) 

| Service      | USB Path |
| ------------- | ------------- |
| Itemzflow main app     | /mnt/usb<USB_NUMBER>/itemzflow/crash.log  |
| Dumper    | /mnt/usb<USB_NUMBER>/itemzflow/dumper.log  |
| Itemz Loader   | /usb0/itemzflow/loader.log  |
| Itemz Daemon   | /mnt/usb0/itemzflow/daemon.log |



## Official Discord server

Invite: https://discord.gg/GvzDdx9GTc

## Game Patches

- Downloads from [online database](https://github.com/illusion0001/console-game-patches) via the app or manual install via [zip file](https://assets.illusion0001.com/patch1.zip)
- Supported formats: `yaml` filename must be `(TITLE_ID).yml, i.e CUSA00547.yml`
- Startup patches only supports `eboot.bin` for now. PRs are very welcome to support games that use multple elf.
- YML per game filepath: `/data/GoldHEN/patches/yml/(TITLE_ID).yml`

### Patch Syntax

Syntax:
```json
{ "type": "", "addr": "", "value": "" }
```

Types:

- `byte`: 1 byte
- `bytes16`: 2 bytes
- `bytes32`: 4 bytes
- `bytes64`: 8 bytes
- `bytes`: array of bytes (no spaces)
- `utf8`: utf8 string
- `utf16`: utf16 string

Examples:

```json
{ "type": "byte", "addr": "0x01271464", "value": "0x01" },
{ "type": "bytes", "addr": "0x00402d90", "value": "31c0c3" }
```

## Donations

We accept the following methods

- [Ko-fi](https://ko-fi.com/lightningmods)
- BTC: 3MEuZAaA7gfKxh9ai4UwYgHZr5DVWfR6Lw

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
- [Sleirsgoevy](https://github.com/sleirsgoevy/)
- [AlAzif](https://github.com/al-azif)
- BigBoss
- [bucanero](https://github.com/bucanero) (Game Saves)
- [illusion](https://github.com/illusion0001) (Game Patches/Trainers)
