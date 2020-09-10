// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <asl/Date.h>
#include <asl/File.h>
#include <asl/Process.h>
#include <asl/String.h>
#include <clip.h>
#include <cryptopp/osrng.h>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>
#include <imgui_stdlib.h>
#include <portable-file-dialogs.h>
#ifdef HAVE_CUBEB
#include "audio_core/cubeb_input.h"
#endif
#include <stb_image_write.h>
#include "audio_core/sink.h"
#include "audio_core/sink_details.h"
#include "common/file_util.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "common/texture.h"
#include "core/3ds.h"
#include "core/cheats/cheat_base.h"
#include "core/cheats/cheats.h"
#include "core/core.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/archive_source_sd_savedata.h"
#include "core/file_sys/ncch_container.h"
#include "core/hle/applets/mii_selector.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/movie.h"
#include "core/settings.h"
#include "input_common/keyboard.h"
#include "input_common/main.h"
#include "input_common/motion_emu.h"
#include "input_common/sdl/sdl.h"
#include "network/room_member.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/texture_filters/texture_filterer.h"
#include "video_core/video_core.h"
#include "vvctre/common.h"
#include "vvctre/emu_window/emu_window_sdl2.h"
#include "vvctre/plugins.h"

static bool is_open = true;

// From https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/database/amiibo.json
static const std::unordered_map<u8, std::string> amiibo_series = {
    {0x00, "Super Smash Bros."},
    {0x01, "Super Mario Bros."},
    {0x02, "Chibi-Robo!"},
    {0x03, "Yoshi's Woolly World"},
    {0x04, "Splatoon"},
    {0x05, "Animal Crossing"},
    {0x06, "8-bit Mario"},
    {0x07, "Skylanders"},
    {0x09, "Legend Of Zelda"},
    {0x10, "BoxBoy!"},
    {0x11, "Pikmin"},
    {0x12, "Fire Emblem"},
    {0x13, "Metroid"},
    {0x14, "Others"},
    {0x15, "Mega Man"},
    {0x16, "Diablo"},
    {0x17, "Power Pros"},
    {0x0a, "Shovel Knight"},
    {0x0c, "Kirby"},
    {0x0d, "Pokemon"},
    {0x0e, "Mario Sports Superstars"},
    {0x0f, "Monster Hunter"},
};
static const std::vector<std::tuple<u64, std::string>> amiibos = {
    {0x0000000000000002, "Mario"},
    {0x0000000000340102, "Mario"},
    {0x00000000003c0102, "Mario - Gold Edition"},
    {0x00000000003d0102, "Mario - Silver Edition"},
    {0x0000000002380602, "8-Bit Mario Classic Color"},
    {0x0000000002390602, "8-Bit Mario Modern Color"},
    {0x0000000003710102, "Mario - Wedding"},
    {0x0000010000190002, "Dr. Mario"},
    {0x00010000000c0002, "Luigi"},
    {0x0001000000350102, "Luigi"},
    {0x0002000000010002, "Peach"},
    {0x0002000000360102, "Peach"},
    {0x0002000003720102, "Peach - Wedding"},
    {0x0003000000020002, "Yoshi"},
    {0x0003000000370102, "Yoshi"},
    {0x0003010200410302, "Green Yarn Yoshi"},
    {0x0003010200420302, "Pink Yarn Yoshi"},
    {0x0003010200430302, "Light Blue Yarn Yoshi"},
    {0x00030102023e0302, "Mega Yarn Yoshi"},
    {0x0004000002620102, "Rosalina"},
    {0x0004010000130002, "Rosalina & Luma"},
    {0x0005000000140002, "Bowser"},
    {0x0005000000390102, "Bowser"},
    {0x0005000003730102, "Bowser - Wedding"},
    {0x0005ff00023a0702, "Hammer Slam Bowser"},
    {0x0006000000150002, "Bowser Jr."},
    {0x00070000001a0002, "Wario"},
    {0x0007000002630102, "Wario"},
    {0x0008000000030002, "Donkey Kong"},
    {0x0008000002640102, "Donkey Kong"},
    {0x0008ff00023b0702, "Turbo Charge Donkey Kong"},
    {0x00090000000d0002, "Diddy Kong"},
    {0x0009000002650102, "Diddy Kong"},
    {0x000a000000380102, "Toad"},
    {0x0013000002660102, "Daisy"},
    {0x00130000037a0002, "Daisy"},
    {0x0014000002670102, "Waluigi"},
    {0x0017000002680102, "Boo"},
    {0x00800102035d0302, "Poochy"},
    {0x0100000000040002, "Link"},
    {0x01000000034b0902, "Link - Ocarina of Time"},
    {0x01000000034c0902, "Link - Majora's Mask"},
    {0x01000000034d0902, "Link - Twilight Princess"},
    {0x01000000034e0902, "Link - Skyward Sword"},
    {0x01000000034f0902, "8-Bit Link"},
    {0x0100000003530902, "Link - Archer"},
    {0x0100000003540902, "Link - Rider"},
    {0x01000000037c0002, "Young Link"},
    {0x0100000003990902, "Link - Link's Awakening"},
    {0x0100010000160002, "Toon Link"},
    {0x0100010003500902, "Toon Link - The Wind Waker"},
    {0x01010000000e0002, "Zelda"},
    {0x0101000003520902, "Toon Zelda - The Wind Waker"},
    {0x0101000003560902, "Zelda"},
    {0x0101010000170002, "Sheik"},
    {0x01020100001b0002, "Ganondorf"},
    {0x01030000024f0902, "Midna & Wolf Link"},
    {0x0105000003580902, "Daruk"},
    {0x0106000003590902, "Urbosa"},
    {0x01070000035a0902, "Mipha"},
    {0x01080000035b0902, "Revali"},
    {0x0140000003550902, "Guardian"},
    {0x01410000035c0902, "Bokoblin"},
    {0x0180000000080002, "Villager"},
    {0x01810000024b0502, "Isabelle - Summer Outfit"},
    {0x0181000100440502, "Isabelle"},
    {0x01810000037d0002, "Isabelle"},
    {0x0181000101d40502, "Isabelle - Character Parfait"},
    {0x01810100023f0502, "Isabelle  - Winter"},
    {0x0181010100b40502, "Isabelle - Winter"},
    {0x01810201011a0502, "Isabelle - Kimono"},
    {0x0181030101700502, "Isabelle - Dress"},
    {0x0182000002400502, "K. K. Slider"},
    {0x0182000100a80502, "K.K. Slider"},
    {0x0182000101d80502, "K. K. Slider - Pikopuri"},
    {0x0182010100460502, "DJ KK"},
    {0x0183000002420502, "Tom Nook"},
    {0x0183000100450502, "Tom Nook"},
    {0x01830101010e0502, "Tom Nook - Jacket"},
    {0x01840000024d0502, "Timmy & Tommy"},
    {0x01850001004b0502, "Timmy"},
    {0x0185020101170502, "Timmy - Full Apron"},
    {0x0185040101790502, "Timmy - Suit"},
    {0x0186010100af0502, "Tommy - Uniform"},
    {0x0186030101750502, "Tommy - Suit"},
    {0x0187000100470502, "Sable"},
    {0x0188000002410502, "Mabel"},
    {0x0188000101120502, "Mabel"},
    {0x0189000100ab0502, "Labelle"},
    {0x018a000002450502, "Reese"},
    {0x018a000100a90502, "Reese"},
    {0x018b000002460502, "Cyrus"},
    {0x018b000101150502, "Cyrus"},
    {0x018c000002430502, "Digby"},
    {0x018c0001004c0502, "Digby"},
    {0x018c010101180502, "Digby - Raincoat"},
    {0x018d0000024c0502, "Rover"},
    {0x018d0001010c0502, "Rover"},
    {0x018e000002490502, "Resetti"},
    {0x018e000100490502, "Resetti"},
    {0x018e010101780502, "Resetti - Without Hat"},
    {0x018f000100b30502, "Don Resetti"},
    {0x018f010101190502, "Don Resetti - Without Hat"},
    {0x0190000101710502, "Brewster"},
    {0x01910001004e0502, "Harriet"},
    {0x0192000002470502, "Blathers"},
    {0x01920001010d0502, "Blathers"},
    {0x0193000002480502, "Celeste"},
    {0x0193000101740502, "Celeste"},
    {0x01940000024a0502, "Kicks"},
    {0x0194000100aa0502, "Kicks"},
    {0x0195000100b00502, "Porter"},
    {0x01960000024e0502, "Kapp'n"},
    {0x0196000100480502, "Kapp'n"},
    {0x0197000101770502, "Leilani"},
    {0x0198000100b10502, "Lelia"},
    {0x0199000101160502, "Grams"},
    {0x019a000100b70502, "Chip"},
    {0x019b000100b60502, "Nat"},
    {0x019c000101730502, "Phineas"},
    {0x019d000100ac0502, "Copper"},
    {0x019e000100ad0502, "Booker"},
    {0x019f000101110502, "Pete"},
    {0x01a00001010f0502, "Pelly"},
    {0x01a1000101100502, "Phyllis"},
    {0x01a20001017d0502, "Gulliver"},
    {0x01a30001004a0502, "Joan"},
    {0x01a40001004d0502, "Pascal"},
    {0x01a5000101720502, "Katrina"},
    {0x01a6000100500502, "Sahara"},
    {0x01a7000101140502, "Wendell"},
    {0x01a80001004f0502, "Redd"},
    {0x01a80101017e0502, "Redd - Shirt"},
    {0x01a9000101760502, "Gracie"},
    {0x01aa000100530502, "Lyle"},
    {0x01ab0001017c0502, "Pave"},
    {0x01ac0001017f0502, "Zipper"},
    {0x01ad000100b80502, "Jack"},
    {0x01ae0001011b0502, "Franklin"},
    {0x01af0001011c0502, "Jingle"},
    {0x01b0000100520502, "Tortimer"},
    {0x01b1000100b20502, "Shrunk"},
    {0x01b10101017b0502, "Shrunk - Loud Jacket"},
    {0x01b3000100b50502, "Blanca"},
    {0x01b4000101130502, "Leif"},
    {0x01b5000100510502, "Luna"},
    {0x01b6000100ae0502, "Katie"},
    {0x01c1000002440502, "Lottie"},
    {0x01c1000100540502, "Lottie"},
    {0x01c10101017a0502, "Lottie - Black Skirt And Bow"},
    {0x0200000100a10502, "Cyrano"},
    {0x02010001016a0502, "Antonio"},
    {0x0202000101030502, "Pango"},
    {0x02030001019a0502, "Anabelle"},
    {0x0206000103120502, "Snooty"},
    {0x0208000100960502, "Annalisa"},
    {0x02090001019f0502, "Olaf"},
    {0x0214000100e40502, "Teddy"},
    {0x0215000101820502, "Pinky"},
    {0x0216000100570502, "Curt"},
    {0x0217000101b30502, "Chow"},
    {0x02190001007e0502, "Nate"},
    {0x021a000100da0502, "Groucho"},
    {0x021b000100800502, "Tutu"},
    {0x021c000102f70502, "Ursala"},
    {0x021d000101cd0502, "Grizzly"},
    {0x021e000101230502, "Paula"},
    {0x021f000103170502, "Ike"},
    {0x0220000100fd0502, "Charlise"},
    {0x02210001013c0502, "Beardo"},
    {0x0222000101440502, "Klaus"},
    {0x022d000100f20502, "Jay"},
    {0x022e000101d30502, "Robin"},
    {0x022f0001011e0502, "Anchovy"},
    {0x0230000101d20502, "Twiggy"},
    {0x02310001006a0502, "Jitters"},
    {0x0232000102ea0502, "Piper"},
    {0x0233000103060502, "Admiral"},
    {0x0235000100840502, "Midge"},
    {0x0238000102f80502, "Jacob"},
    {0x023c000100bd0502, "Lucha"},
    {0x023d000101b50502, "Jacques"},
    {0x023e000100d10502, "Peck"},
    {0x023f000101660502, "Sparro"},
    {0x024a000101d10502, "Angus"},
    {0x024b000101260502, "Rodeo"},
    {0x024d000102f60502, "Stu"},
    {0x024f000100810502, "T-Bone"},
    {0x0251000100c10502, "Coach"},
    {0x0252000100fe0502, "Vic"},
    {0x025d000100550502, "Bob"},
    {0x025e000101250502, "Mitzi"},
    {0x025f000101c50502, "Rosie"},
    {0x025f000101d70502, "Rosie - Amiibo Festival"},
    {0x0260000100d20502, "Olivia"},
    {0x0261000100650502, "Kiki"},
    {0x0262000101370502, "Tangy"},
    {0x0263000100750502, "Punchy"},
    {0x0264000101ac0502, "Purrl"},
    {0x0265000101540502, "Moe"},
    {0x0266000100680502, "Kabuki"},
    {0x0267000101080502, "Kid Cat"},
    {0x02680001007d0502, "Monique"},
    {0x02690001011f0502, "Tabby"},
    {0x026a000101460502, "Stinky"},
    {0x026b000100e90502, "Kitty"},
    {0x026c000100c30502, "Tom"},
    {0x026d0001013f0502, "Merry"},
    {0x026e000100ba0502, "Felicity"},
    {0x026f000101900502, "Lolly"},
    {0x0270000100ff0502, "Ankha"},
    {0x02710001019b0502, "Rudy"},
    {0x0272000101860502, "Katt"},
    {0x027d000100630502, "Bluebear"},
    {0x027e000101690502, "Maple"},
    {0x027f000100b90502, "Poncho"},
    {0x0280000100830502, "Pudge"},
    {0x0281000101200502, "Kody"},
    {0x0282000101810502, "Stitches"},
    {0x0282000101d60502, "Stitches - Amiibo Festival"},
    {0x0283000100c70502, "Vladimir"},
    {0x0284000102fe0502, "Murphy"},
    {0x0286000103130502, "Olive"},
    {0x02870001005a0502, "Cheri"},
    {0x028a000102e90502, "June"},
    {0x028b000100e30502, "Pekoe"},
    {0x028c0001013e0502, "Chester"},
    {0x028d000101bd0502, "Barold"},
    {0x028e0001019e0502, "Tammy"},
    {0x028f0101031a0502, "Marty"},
    {0x0299000100950502, "Goose"},
    {0x029a000100ee0502, "Benedict"},
    {0x029b000100cb0502, "Egbert"},
    {0x029e0001013d0502, "Ava"},
    {0x02a2000101ba0502, "Becky"},
    {0x02a3000102ff0502, "Plucky"},
    {0x02a4000100720502, "Knox"},
    {0x02a50001018c0502, "Broffina"},
    {0x02a6000101240502, "Ken"},
    {0x02b1000100690502, "Patty"},
    {0x02b2000100c40502, "Tipper"},
    {0x02b70001030f0502, "Norma"},
    {0x02b80001019c0502, "Naomi"},
    {0x02c3000100dc0502, "Alfonso"},
    {0x02c4000100670502, "Alli"},
    {0x02c5000103080502, "Boots"},
    {0x02c7000101220502, "Del"},
    {0x02c9000100cd0502, "Sly"},
    {0x02ca000101ca0502, "Gayle"},
    {0x02cb000101360502, "Drago"},
    {0x02d6000100560502, "Fauna"},
    {0x02d7000101300502, "Bam"},
    {0x02d8000100e20502, "Zell"},
    {0x02d9000101c80502, "Bruce"},
    {0x02da000101330502, "Deirdre"},
    {0x02db0001005e0502, "Lopez"},
    {0x02dc000100be0502, "Fuchsia"},
    {0x02dd000100ea0502, "Beau"},
    {0x02de0001009c0502, "Diana"},
    {0x02df000101910502, "Erik"},
    {0x02e00101031d0502, "Chelsea"},
    {0x02ea000101800502, "Goldie"},
    {0x02ea000101d50502, "Goldie - Amiibo Festival"},
    {0x02eb000100de0502, "Butch"},
    {0x02ec000101c40502, "Lucky"},
    {0x02ed0001015a0502, "Biskit"},
    {0x02ee000101990502, "Bones"},
    {0x02ef000100580502, "Portia"},
    {0x02f0000100a70502, "Walker"},
    {0x02f1000101450502, "Daisy"},
    {0x02f2000100cc0502, "Cookie"},
    {0x02f3000102f90502, "Maddie"},
    {0x02f4000103050502, "Bea"},
    {0x02f8000101380502, "Mac"},
    {0x02f9000101020502, "Marcel"},
    {0x02fa000100970502, "Benjamin"},
    {0x02fb000100900502, "Cherry"},
    {0x02fc0001018f0502, "Shep"},
    {0x0307000100640502, "Bill"},
    {0x03080001014d0502, "Joey"},
    {0x0309000100c60502, "Pate"},
    {0x030a000101c70502, "Maelle"},
    {0x030b000100790502, "Deena"},
    {0x030c000101b80502, "Pompom"},
    {0x030d000101840502, "Mallary"},
    {0x030e0001012f0502, "Freckles"},
    {0x030f0001016d0502, "Derwin"},
    {0x0310000100f80502, "Drake"},
    {0x0311000100d60502, "Scoot"},
    {0x0312000103090502, "Weber"},
    {0x0313000101210502, "Miranda"},
    {0x0314000102f40502, "Ketchup"},
    {0x0316000101c00502, "Gloria"},
    {0x0317000100a60502, "Molly"},
    {0x03180001006c0502, "Quillson"},
    {0x0323000100760502, "Opal"},
    {0x0324000101890502, "Dizzy"},
    {0x03250001010a0502, "Big Top"},
    {0x0326000101390502, "Eloise"},
    {0x0327000101c30502, "Margie"},
    {0x0328000102eb0502, "Paolo"},
    {0x03290001009d0502, "Axel"},
    {0x032a000103070502, "Ellie"},
    {0x032c000101480502, "Tucker"},
    {0x032d000100bc0502, "Tia"},
    {0x032e0101031c0502, "Chai"},
    {0x03380001011d0502, "Lily"},
    {0x0339000101b10502, "Ribbot"},
    {0x033a000101cc0502, "Frobert"},
    {0x033b000100fa0502, "Camofrog"},
    {0x033c000101000502, "Drift"},
    {0x033d0001013a0502, "Wart Jr."},
    {0x033e000101a20502, "Puddles"},
    {0x033f0001008f0502, "Jeremiah"},
    {0x03410001030e0502, "Tad"},
    {0x0342000101280502, "Cousteau"},
    {0x0343000102ef0502, "Huck"},
    {0x0344000100c50502, "Prince"},
    {0x03450001005f0502, "Jambette"},
    {0x0347000103020502, "Raddle"},
    {0x03480001006b0502, "Gigi"},
    {0x03490001018d0502, "Croque"},
    {0x034a000101430502, "Diva"},
    {0x034b0001009f0502, "Henry"},
    {0x0356000101350502, "Chevre"},
    {0x0357000100eb0502, "Nan"},
    {0x0358000102fa0502, "Billy"},
    {0x035a000100850502, "Gruff"},
    {0x035c000101290502, "Velma"},
    {0x035d000100c90502, "Kidd"},
    {0x035e0001018e0502, "Pashmina"},
    {0x0369000100d30502, "Cesar"},
    {0x036a0001019d0502, "Peewee"},
    {0x036b0001018b0502, "Boone"},
    {0x036d000103040502, "Louie"},
    {0x036e000102fb0502, "Boyd"},
    {0x03700001015d0502, "Violet"},
    {0x03710001005c0502, "Al"},
    {0x03720001010b0502, "Rocket"},
    {0x0373000101340502, "Hans"},
    {0x0374010103190502, "Rilla"},
    {0x037e000101560502, "Hamlet"},
    {0x037f000101aa0502, "Apple"},
    {0x0380000101870502, "Graham"},
    {0x0381000100d50502, "Rodney"},
    {0x03820001016b0502, "Soleil"},
    {0x03830001009b0502, "Clay"},
    {0x0384000100860502, "Flurry"},
    {0x0385000101060502, "Hamphrey"},
    {0x0390000101850502, "Rocco"},
    {0x0392000101270502, "Bubbles"},
    {0x0393000100a00502, "Bertha"},
    {0x0394000100890502, "Biff"},
    {0x0395000102fc0502, "Bitty"},
    {0x0398000100bf0502, "Harry"},
    {0x0399000101c20502, "Hippeux"},
    {0x03a40001014f0502, "Buck"},
    {0x03a50001015b0502, "Victoria"},
    {0x03a6000100c80502, "Savannah"},
    {0x03a7000101a10502, "Elmer"},
    {0x03a8000100910502, "Rosco"},
    {0x03a9000100710502, "Winnie"},
    {0x03aa000100e60502, "Ed"},
    {0x03ab000103160502, "Cleo"},
    {0x03ac000101880502, "Peaches"},
    {0x03ad000101b20502, "Annalise"},
    {0x03ae000100870502, "Clyde"},
    {0x03af0001012c0502, "Colton"},
    {0x03b0000101a90502, "Papi"},
    {0x03b1000100f00502, "Julian"},
    {0x03bc0001008a0502, "Yuka"},
    {0x03bd000100f90502, "Alice"},
    {0x03be000101980502, "Melba"},
    {0x03bf000101bc0502, "Sydney"},
    {0x03c0000103100502, "Gonzo"},
    {0x03c1000100bb0502, "Ozzie"},
    {0x03c40001012b0502, "Canberra"},
    {0x03c50001015c0502, "Lyman"},
    {0x03c6000100930502, "Eugene"},
    {0x03d1000100c20502, "Kitt"},
    {0x03d2000100e50502, "Mathilda"},
    {0x03d3000102f30502, "Carrie"},
    {0x03d6000101570502, "Astrid"},
    {0x03d7000101b40502, "Sylvia"},
    {0x03d9000101a50502, "Walt"},
    {0x03da000101510502, "Rooney"},
    {0x03db0001006d0502, "Marcie"},
    {0x03e6000100ec0502, "Bud"},
    {0x03e70001012a0502, "Elvis"},
    {0x03e8000102f50502, "Rex"},
    {0x03ea0001030b0502, "Leopold"},
    {0x03ec000101830502, "Mott"},
    {0x03ed000101a30502, "Rory"},
    {0x03ee0001008b0502, "Lionel"},
    {0x03fa000100d00502, "Nana"},
    {0x03fb000101cf0502, "Simon"},
    {0x03fc000101470502, "Tammi"},
    {0x03fd000101580502, "Monty"},
    {0x03fe000101a40502, "Elise"},
    {0x03ff000100f40502, "Flip"},
    {0x04000001006f0502, "Shari"},
    {0x0401000100660502, "Deli"},
    {0x040c000101590502, "Dora"},
    {0x040d000100780502, "Limberg"},
    {0x040e000100880502, "Bella"},
    {0x040f000101500502, "Bree"},
    {0x04100001007f0502, "Samson"},
    {0x0411000101ab0502, "Rod"},
    {0x04140001030a0502, "Candi"},
    {0x0415000101bb0502, "Rizzo"},
    {0x0416000100fb0502, "Anicotti"},
    {0x0418000100d80502, "Broccolo"},
    {0x041a000100e00502, "Moose"},
    {0x041b000100f10502, "Bettina"},
    {0x041c000101410502, "Greta"},
    {0x041d0001018a0502, "Penelope"},
    {0x041e0001015f0502, "Chadder"},
    {0x0429000100700502, "Octavian"},
    {0x042a0001012d0502, "Marina"},
    {0x042b000101af0502, "Zucker"},
    {0x0436000101940502, "Queenie"},
    {0x0437000101050502, "Gladys"},
    {0x0438000103000502, "Sandy"},
    {0x0439000103110502, "Sprocket"},
    {0x043b000103030502, "Julia"},
    {0x043c000101cb0502, "Cranston"},
    {0x043d0001007c0502, "Phil"},
    {0x043e000101490502, "Blanche"},
    {0x043f000101550502, "Flora"},
    {0x0440000100ca0502, "Phoebe"},
    {0x044b0001016c0502, "Apollo"},
    {0x044c0001008e0502, "Amelia"},
    {0x044d000101930502, "Pierce"},
    {0x044e000103150502, "Buzz"},
    {0x0450000100cf0502, "Avery"},
    {0x04510001015e0502, "Frank"},
    {0x0452000100730502, "Sterling"},
    {0x0453000101040502, "Keaton"},
    {0x0454000101ae0502, "Celia"},
    {0x045f000101a80502, "Aurora"},
    {0x0460000100a50502, "Roald"},
    {0x0461000101610502, "Cube"},
    {0x0462000100f60502, "Hopper"},
    {0x0463000101310502, "Friga"},
    {0x0464000100c00502, "Gwen"},
    {0x04650001006e0502, "Puck"},
    {0x0468000102f20502, "Wade"},
    {0x0469000101640502, "Boomer"},
    {0x046a000101d00502, "Iggly"},
    {0x046b000101970502, "Tex"},
    {0x046c0001008c0502, "Flo"},
    {0x046d000100f30502, "Sprinkle"},
    {0x0478000101630502, "Curly"},
    {0x0479000100920502, "Truffles"},
    {0x047a000100600502, "Rasher"},
    {0x047b000100f50502, "Hugh"},
    {0x047c000101a00502, "Lucy"},
    {0x047d0001012e0502, "Spork/Crackle"},
    {0x04800001008d0502, "Cobb"},
    {0x0481000102f10502, "Boris"},
    {0x0482000102fd0502, "Maggie"},
    {0x0483000101b00502, "Peggy"},
    {0x04850001014c0502, "Gala"},
    {0x0486000100fc0502, "Chops"},
    {0x0487000101bf0502, "Kevin"},
    {0x0488000100980502, "Pancetti"},
    {0x0489000100ef0502, "Agnes"},
    {0x04940001009a0502, "Bunnie"},
    {0x0495000101920502, "Dotty"},
    {0x0496000100d90502, "Coco"},
    {0x04970001007a0502, "Snake"},
    {0x04980001014a0502, "Gaston"},
    {0x0499000100df0502, "Gabi"},
    {0x049a0001014e0502, "Pippy"},
    {0x049b000100610502, "Tiffany"},
    {0x049c000101400502, "Genji"},
    {0x049d000100ed0502, "Ruby"},
    {0x049e000101b70502, "Doc"},
    {0x049f000103010502, "Claude"},
    {0x04a00001016e0502, "Francine"},
    {0x04a10001016f0502, "Chrissy"},
    {0x04a2000102e80502, "Hopkins"},
    {0x04a3000101c90502, "OHare"},
    {0x04a4000100d40502, "Carmen"},
    {0x04a5000100740502, "Bonbon"},
    {0x04a6000100a30502, "Cole"},
    {0x04a7000101a60502, "Mira"},
    {0x04a80101031e0502, "Toby"},
    {0x04b2000101b90502, "Tank"},
    {0x04b3000100dd0502, "Rhonda"},
    {0x04b40001030c0502, "Spike"},
    {0x04b6000102ec0502, "Hornsby"},
    {0x04b9000101600502, "Merengue"},
    {0x04ba0001005d0502, "Renée"},
    {0x04c5000101010502, "Vesta"},
    {0x04c6000101670502, "Baabara"},
    {0x04c7000100940502, "Eunice"},
    {0x04c8000102ed0502, "Stella"},
    {0x04c90001030d0502, "Cashmere"},
    {0x04cc000100a40502, "Willow"},
    {0x04cd000101520502, "Curlos"},
    {0x04ce000100db0502, "Wendy"},
    {0x04cf000100e10502, "Timbra"},
    {0x04d0000101960502, "Frita"},
    {0x04d10001009e0502, "Muffy"},
    {0x04d2000101a70502, "Pietro"},
    {0x04d30101031b0502, "Étoile"},
    {0x04dd000100a20502, "Peanut"},
    {0x04de000100ce0502, "Blaire"},
    {0x04df000100e80502, "Filbert"},
    {0x04e0000100f70502, "Pecan"},
    {0x04e1000101be0502, "Nibbles"},
    {0x04e2000101090502, "Agent S"},
    {0x04e3000101650502, "Caroline"},
    {0x04e4000101b60502, "Sally"},
    {0x04e5000101ad0502, "Static"},
    {0x04e6000100820502, "Mint"},
    {0x04e7000101320502, "Ricky"},
    {0x04e8000101ce0502, "Cally"},
    {0x04ea000103180502, "Tasha"},
    {0x04eb000102f00502, "Sylvana"},
    {0x04ec000100770502, "Poppy"},
    {0x04ed000100620502, "Sheldon"},
    {0x04ee0001014b0502, "Marshal"},
    {0x04ef0001013b0502, "Hazel"},
    {0x04fa000101680502, "Rolf"},
    {0x04fb000101c60502, "Rowan"},
    {0x04fc000102ee0502, "Tybalt"},
    {0x04fd0001007b0502, "Bangle"},
    {0x04fe000100590502, "Leonardo"},
    {0x04ff000101620502, "Claudia"},
    {0x0500000100e70502, "Bianca"},
    {0x050b000100990502, "Chief"},
    {0x050c000101c10502, "Lobo"},
    {0x050d000101420502, "Wolfgang"},
    {0x050e000100d70502, "Whitney"},
    {0x050f000103140502, "Dobie"},
    {0x0510000101070502, "Freya"},
    {0x0511000101950502, "Fang"},
    {0x0513000102e70502, "Vivian"},
    {0x0514000101530502, "Skye"},
    {0x05150001005b0502, "Kyle"},
    {0x0580000000050002, "Fox"},
    {0x05810000001c0002, "Falco"},
    {0x05840000037e0002, "Wolf"},
    {0x05c0000000060002, "Samus"},
    {0x05c0000003651302, "Samus Aran"},
    {0x05c00100001d0002, "Zero Suit Samus"},
    {0x05c1000003661302, "Metroid"},
    {0x05c20000037f0002, "Ridley"},
    {0x05c3000003800002, "Dark Samus"},
    {0x0600000000120002, "Captain Falcon"},
    {0x06400100001e0002, "Olimar"},
    {0x06c00000000f0002, "Little Mac"},
    {0x0700000000070002, "Wii Fit Trainer"},
    {0x0740000000100002, "Pit"},
    {0x0741000000200002, "Dark Pit"},
    {0x07420000001f0002, "Palutena"},
    {0x07800000002d0002, "Mr. Game & Watch"},
    {0x07810000002e0002, "R.O.B - Famicom"},
    {0x0781000000330002, "R.O.B. - NES"},
    {0x07820000002f0002, "Duck Hunt"},
    {0x07c0000000210002, "Mii Brawler"},
    {0x07c0010000220002, "Mii Swordfighter"},
    {0x07c0020000230002, "Mii Gunner"},
    {0x08000100003e0402, "Inkling Girl"},
    {0x08000100025f0402, "Inkling Girl - Lime Green"},
    {0x0800010003690402, "Inkling Girl - Neon Pink"},
    {0x0800010003820002, "Inkling"},
    {0x08000200003f0402, "Inkling Boy"},
    {0x0800020002600402, "Inkling Boy - Purple"},
    {0x08000200036a0402, "Inkling Boy - Neon Green"},
    {0x0800030000400402, "Inkling Squid"},
    {0x0800030002610402, "Inkling Squid - Orange"},
    {0x08000300036b0402, "Inkling Squid - Neon Purple"},
    {0x08010000025d0402, "Callie"},
    {0x08020000025e0402, "Marie"},
    {0x0803000003760402, "Pearl"},
    {0x0804000003770402, "Marina"},
    {0x08050100038e0402, "Octoling Girl"},
    {0x08050200038f0402, "Octoling Boy"},
    {0x0805030003900402, "Octoling Octopus"},
    {0x09c0010102690e02, "Mario - Soccer"},
    {0x09c00201026a0e02, "Mario - Baseball"},
    {0x09c00301026b0e02, "Mario - Tennis"},
    {0x09c00401026c0e02, "Mario - Golf"},
    {0x09c00501026d0e02, "Mario - Horse Racing"},
    {0x09c10101026e0e02, "Luigi - Soccer"},
    {0x09c10201026f0e02, "Luigi - Baseball"},
    {0x09c1030102700e02, "Luigi - Tennis"},
    {0x09c1040102710e02, "Luigi - Golf"},
    {0x09c1050102720e02, "Luigi - Horse Racing"},
    {0x09c2010102730e02, "Peach - Soccer"},
    {0x09c2020102740e02, "Peach - Baseball"},
    {0x09c2030102750e02, "Peach - Tennis"},
    {0x09c2040102760e02, "Peach - Golf"},
    {0x09c2050102770e02, "Peach - Horse Racing"},
    {0x09c3010102780e02, "Daisy - Soccer"},
    {0x09c3020102790e02, "Daisy - Baseball"},
    {0x09c30301027a0e02, "Daisy - Tennis"},
    {0x09c30401027b0e02, "Daisy - Golf"},
    {0x09c30501027c0e02, "Daisy - Horse Racing"},
    {0x09c40101027d0e02, "Yoshi - Soccer"},
    {0x09c40201027e0e02, "Yoshi - Baseball"},
    {0x09c40301027f0e02, "Yoshi - Tennis"},
    {0x09c4040102800e02, "Yoshi - Golf"},
    {0x09c4050102810e02, "Yoshi - Horse Racing"},
    {0x09c5010102820e02, "Wario - Soccer"},
    {0x09c5020102830e02, "Wario - Baseball"},
    {0x09c5030102840e02, "Wario - Tennis"},
    {0x09c5040102850e02, "Wario - Golf"},
    {0x09c5050102860e02, "Wario - Horse Racing"},
    {0x09c6010102870e02, "Waluigi - Soccer"},
    {0x09c6020102880e02, "Waluigi - Baseball"},
    {0x09c6030102890e02, "Waluigi - Tennis"},
    {0x09c60401028a0e02, "Waluigi - Golf"},
    {0x09c60501028b0e02, "Waluigi - Horse Racing"},
    {0x09c70101028c0e02, "Donkey Kong - Soccer"},
    {0x09c70201028d0e02, "Donkey Kong - Baseball"},
    {0x09c70301028e0e02, "Donkey Kong - Tennis"},
    {0x09c70401028f0e02, "Donkey Kong - Golf"},
    {0x09c7050102900e02, "Donkey Kong - Horse Racing"},
    {0x09c8010102910e02, "Diddy Kong - Soccer"},
    {0x09c8020102920e02, "Diddy Kong - Baseball"},
    {0x09c8030102930e02, "Diddy Kong - Tennis"},
    {0x09c8040102940e02, "Diddy Kong - Golf"},
    {0x09c8050102950e02, "Diddy Kong - Horse Racing"},
    {0x09c9010102960e02, "Bowser - Soccer"},
    {0x09c9020102970e02, "Bowser - Baseball"},
    {0x09c9030102980e02, "Bowser - Tennis"},
    {0x09c9040102990e02, "Bowser - Golf"},
    {0x09c90501029a0e02, "Bowser - Horse Racing"},
    {0x09ca0101029b0e02, "Bowser Jr. - Soccer"},
    {0x09ca0201029c0e02, "Bowser Jr. - Baseball"},
    {0x09ca0301029d0e02, "Bowser Jr. - Tennis"},
    {0x09ca0401029e0e02, "Bowser Jr. - Golf"},
    {0x09ca0501029f0e02, "Bowser Jr. - Horse Racing"},
    {0x09cb010102a00e02, "Boo - Soccer"},
    {0x09cb020102a10e02, "Boo - Baseball"},
    {0x09cb030102a20e02, "Boo - Tennis"},
    {0x09cb040102a30e02, "Boo - Golf"},
    {0x09cb050102a40e02, "Boo - Horse Racing"},
    {0x09cc010102a50e02, "Baby Mario - Soccer"},
    {0x09cc020102a60e02, "Baby Mario - Baseball"},
    {0x09cc030102a70e02, "Baby Mario - Tennis"},
    {0x09cc040102a80e02, "Baby Mario - Golf"},
    {0x09cc050102a90e02, "Baby Mario - Horse Racing"},
    {0x09cd010102aa0e02, "Baby Luigi - Soccer"},
    {0x09cd020102ab0e02, "Baby Luigi - Baseball"},
    {0x09cd030102ac0e02, "Baby Luigi - Tennis"},
    {0x09cd040102ad0e02, "Baby Luigi - Golf"},
    {0x09cd050102ae0e02, "Baby Luigi - Horse Racing"},
    {0x09ce010102af0e02, "Birdo - Soccer"},
    {0x09ce020102b00e02, "Birdo - Baseball"},
    {0x09ce030102b10e02, "Birdo - Tennis"},
    {0x09ce040102b20e02, "Birdo - Golf"},
    {0x09ce050102b30e02, "Birdo - Horse Racing"},
    {0x09cf010102b40e02, "Rosalina - Soccer"},
    {0x09cf020102b50e02, "Rosalina - Baseball"},
    {0x09cf030102b60e02, "Rosalina - Tennis"},
    {0x09cf040102b70e02, "Rosalina - Golf"},
    {0x09cf050102b80e02, "Rosalina - Horse Racing"},
    {0x09d0010102b90e02, "Metal Mario - Soccer"},
    {0x09d0020102ba0e02, "Metal Mario - Baseball"},
    {0x09d0030102bb0e02, "Metal Mario - Tennis"},
    {0x09d0040102bc0e02, "Metal Mario - Golf"},
    {0x09d0050102bd0e02, "Metal Mario - Horse Racing"},
    {0x09d1010102be0e02, "Pink Gold Peach - Soccer"},
    {0x09d1020102bf0e02, "Pink Gold Peach - Baseball"},
    {0x09d1030102c00e02, "Pink Gold Peach - Tennis"},
    {0x09d1040102c10e02, "Pink Gold Peach - Golf"},
    {0x09d1050102c20e02, "Pink Gold Peach - Horse Racing"},
    {0x1902000003830002, "Ivysaur"},
    {0x1906000000240002, "Charizard"},
    {0x1907000003840002, "Squirtle"},
    {0x1919000000090002, "Pikachu"},
    {0x1927000000260002, "Jigglypuff"},
    {0x19960000023d0002, "Mewtwo"},
    {0x19ac000003850002, "Pichu"},
    {0x1ac0000000110002, "Lucario"},
    {0x1b92000000250002, "Greninja"},
    {0x1bd7000003860002, "Incineroar"},
    {0x1d000001025c0d02, "Shadow Mewtwo"},
    {0x1d01000003750d02, "Detective Pikachu"},
    {0x1d40000003870002, "Pokemon Trainer"},
    {0x1f000000000a0002, "Kirby"},
    {0x1f00000002540c02, "Kirby"},
    {0x1f01000000270002, "Meta Knight"},
    {0x1f01000002550c02, "Meta Knight"},
    {0x1f02000000280002, "King Dedede"},
    {0x1f02000002560c02, "King Dedede"},
    {0x1f03000002570c02, "Waddle Dee"},
    {0x1f400000035e1002, "Qbby"},
    {0x21000000000b0002, "Marth"},
    {0x2101000000180002, "Ike"},
    {0x2102000000290002, "Lucina"},
    {0x21030000002a0002, "Robin"},
    {0x2104000002520002, "Roy"},
    {0x21050000025a0002, "Corrin"},
    {0x2105010003630002, "Corrin - Player 2"},
    {0x2106000003601202, "Alm"},
    {0x2107000003611202, "Celica"},
    {0x2108000003880002, "Chrom"},
    {0x21080000036f1202, "Chrom"},
    {0x2109000003701202, "Tiki"},
    {0x22400000002b0002, "Shulk"},
    {0x22800000002c0002, "Ness"},
    {0x2281000002510002, "Lucas"},
    {0x22c00000003a0202, "Chibi Robo"},
    {0x3200000000300002, "Sonic"},
    {0x32400000025b0002, "Bayonetta"},
    {0x3240010003640002, "Bayonetta - Player 2"},
    {0x3340000000320002, "Pac-Man"},
    {0x3380000003781402, "Solaire of Astora"},
    {0x3480000000310002, "Mega Man"},
    {0x3480000002580002, "Mega Man - Gold Edition"},
    {0x3480000003791502, "Mega Man"},
    {0x34c0000002530002, "Ryu"},
    {0x34c1000003890002, "Ken"},
    {0x3500010002e10f02, "One-Eyed Rathalos and Rider - Male"},
    {0x3500020002e20f02, "One-Eyed Rathalos and Rider - Female"},
    {0x3501000002e30f02, "Nabiru"},
    {0x3502010002e40f02, "Rathian and Cheval"},
    {0x3503010002e50f02, "Barioth and Ayuria"},
    {0x3504010002e60f02, "Qurupeco and Dan"},
    {0x35c0000002500a02, "Shovel Knight"},
    {0x35c0000003920a02, "Shovel Knight - Gold Edition"},
    {0x35c10000036c0a02, "Plague Knight"},
    {0x35c20000036d0a02, "Specter Knight"},
    {0x35c30000036e0a02, "King Knight"},
    {0x3600000002590002, "Cloud"},
    {0x3600010003620002, "Cloud - Player 2"},
    {0x06420000035f1102, "Pikmin"},
    {0x0015000003670102, "Goomba"},
    {0x0023000003680102, "Koopa Troopa"},
    {0x3740000103741402, "Super Mario Cereal"},
    {0x37800000038a0002, "Snake"},
    {0x37c00000038b0002, "Simon"},
    {0x37c10000038c0002, "Richter"},
    {0x3800000103931702, "Pawapuro"},
    {0x3801000103941702, "Ikari"},
    {0x3802000103951702, "Daijobu"},
    {0x3803000103961702, "Hayakawa"},
    {0x3804000103971702, "Yabe"},
    {0x3805000103981702, "Ganda"},
    {0x38c0000003911602, "Loot Goblin"},
    {0x00c00000037b0002, "King K. Rool"},
    {0x00240000038d0002, "Piranha Plant"},
    {0x078f000003810002, "Ice Climbers"},
};

static std::string IPC_Recorder_GetStatusString(IPCDebugger::RequestStatus status) {
    switch (status) {
    case IPCDebugger::RequestStatus::Sent:
        return "Sent";
    case IPCDebugger::RequestStatus::Handling:
        return "Handling";
    case IPCDebugger::RequestStatus::Handled:
        return "Handled";
    case IPCDebugger::RequestStatus::HLEUnimplemented:
        return "HLEUnimplemented";
    default:
        break;
    }

    return "Invalid";
}

void EmuWindow_SDL2::OnMouseMotion(s32 x, s32 y) {
    TouchMoved((unsigned)std::max(x, 0), (unsigned)std::max(y, 0));
    InputCommon::GetMotionEmu()->Tilt(x, y);
}

void EmuWindow_SDL2::OnMouseButton(u32 button, u8 state, s32 x, s32 y) {
    if (button == SDL_BUTTON_LEFT) {
        if (state == SDL_PRESSED) {
            TouchPressed((unsigned)std::max(x, 0), (unsigned)std::max(y, 0));
        } else {
            TouchReleased();
        }
    } else if (button == SDL_BUTTON_RIGHT) {
        if (state == SDL_PRESSED) {
            InputCommon::GetMotionEmu()->BeginTilt(x, y);
        } else {
            InputCommon::GetMotionEmu()->EndTilt();
        }
    }
}

std::pair<unsigned, unsigned> EmuWindow_SDL2::TouchToPixelPos(float touch_x, float touch_y) const {
    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    touch_x *= w;
    touch_y *= h;

    return {static_cast<unsigned>(std::max(std::round(touch_x), 0.0f)),
            static_cast<unsigned>(std::max(std::round(touch_y), 0.0f))};
}

void EmuWindow_SDL2::OnFingerDown(float x, float y) {
    // TODO(NeatNit): keep track of multitouch using the fingerID and a dictionary of some kind
    // This isn't critical because the best we can do when we have that is to average them, like the
    // 3DS does

    const auto [px, py] = TouchToPixelPos(x, y);
    TouchPressed(px, py);
}

void EmuWindow_SDL2::OnFingerMotion(float x, float y) {
    const auto [px, py] = TouchToPixelPos(x, y);
    TouchMoved(px, py);
}

void EmuWindow_SDL2::OnFingerUp() {
    TouchReleased();
}

void EmuWindow_SDL2::OnKeyEvent(int key, u8 state) {
    if (state == SDL_PRESSED) {
        InputCommon::GetKeyboard()->PressKey(key);
    } else if (state == SDL_RELEASED) {
        InputCommon::GetKeyboard()->ReleaseKey(key);
    }
}

bool EmuWindow_SDL2::IsOpen() const {
    return is_open;
}

void EmuWindow_SDL2::Close() {
    is_open = false;
}

void EmuWindow_SDL2::OnResize() {
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    UpdateCurrentFramebufferLayout(width, height);
}

EmuWindow_SDL2::EmuWindow_SDL2(Core::System& system, PluginManager& plugin_manager,
                               SDL_Window* window, bool& ok_multiplayer)
    : window(window), system(system), plugin_manager(plugin_manager) {
    signal(SIGINT, [](int) { is_open = false; });
    signal(SIGTERM, [](int) { is_open = false; });

    Network::RoomMember& room_member = system.RoomMember();

    room_member.BindOnStateChanged([&](const Network::RoomMember::State& state) {
        if (state == Network::RoomMember::State::Idle) {
            multiplayer_message.clear();
            multiplayer_messages.clear();
            multiplayer_blocked_nicknames.clear();
        }
    });

    room_member.BindOnError([&](const Network::RoomMember::Error& error) {
        pfd::message("vvctre", Network::GetErrorStr(error), pfd::choice::ok, pfd::icon::error);
    });

    room_member.BindOnChatMessageReceived([&](const Network::ChatEntry& entry) {
        if (multiplayer_blocked_nicknames.count(entry.nickname)) {
            return;
        }

        asl::Date date = asl::Date::now();
        multiplayer_messages.push_back(fmt::format("[{:02}:{:02}] <{}> {}", date.hours(),
                                                   date.minutes(), entry.nickname, entry.message));
    });

    room_member.BindOnStatusMessageReceived([&](const Network::StatusMessageEntry& entry) {
        if (multiplayer_blocked_nicknames.count(entry.nickname)) {
            return;
        }

        asl::Date date = asl::Date::now();

        switch (entry.type) {
        case Network::StatusMessageTypes::IdMemberJoin:
            multiplayer_messages.push_back(fmt::format("[{:02}:{:02}] {} joined", date.hours(),
                                                       date.minutes(), entry.nickname));
            break;
        case Network::StatusMessageTypes::IdMemberLeave:
            multiplayer_messages.push_back(
                fmt::format("[{:02}:{:02}] {} left", date.hours(), date.minutes(), entry.nickname));
            break;
        case Network::StatusMessageTypes::IdMemberKicked:
            multiplayer_messages.push_back(fmt::format("[{:02}:{:02}] {} was kicked", date.hours(),
                                                       date.minutes(), entry.nickname));
            break;
        case Network::StatusMessageTypes::IdMemberBanned:
            multiplayer_messages.push_back(fmt::format("[{:02}:{:02}] {} was banned", date.hours(),
                                                       date.minutes(), entry.nickname));
            break;
        case Network::StatusMessageTypes::IdAddressUnbanned:
            multiplayer_messages.push_back(
                fmt::format("[{:02}:{:02}] Someone was unbanned", date.hours(), date.minutes()));
            break;
        }
    });

    if (ok_multiplayer) {
        ConnectToCitraRoom();
    }

    SDL_SetWindowTitle(window, fmt::format("vvctre {}.{}.{}", vvctre_version_major,
                                           vvctre_version_minor, vvctre_version_patch)
                                   .c_str());

    SDL_GL_SetSwapInterval(Settings::values.enable_vsync ? 1 : 0);

    OnResize();
    SDL_PumpEvents();
    LOG_INFO(Frontend, "Version: {}.{}.{}", vvctre_version_major, vvctre_version_minor,
             vvctre_version_patch);
    LOG_INFO(Frontend, "Movie version: {}", Core::MovieVersion);
}

EmuWindow_SDL2::~EmuWindow_SDL2() = default;

void EmuWindow_SDL2::SwapBuffers() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    plugin_manager.BeforeDrawingFPS();

    ImGui::SetNextWindowPos(ImVec2(), ImGuiCond_Once);
    if (ImGui::Begin("FPS and Menu", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoFocusOnAppearing)) {
        ImGui::TextColored(fps_color, "%d FPS", static_cast<int>(io.Framerate));
        if (ImGui::BeginPopupContextItem("Menu", ImGuiMouseButton_Right)) {
            if (ImGui::IsWindowAppearing() && !ImGui::IsKeyDown(SDL_SCANCODE_LSHIFT)) {
                paused = true;
            }

            menu_open = true;

            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Load File")) {
                    const std::vector<std::string> result =
                        pfd::open_file("Browse", *asl::Process::myDir(),
                                       {"All supported files",
                                        "*.cci *.CCI *.3ds *.3DS *.cxi *.CXI *.3dsx *.3DSX "
                                        "*.app *.APP *.elf *.ELF *.axf *.AXF",
                                        "Cartridges", "*.cci *.CCI *.3ds *.3DS", "NCCHs",
                                        "*.cxi *.CXI *.app *.APP", "Homebrew",
                                        "*.3dsx *.3DSX *.elf *.ELF *.axf *.AXF"})
                            .result();

                    if (!result.empty()) {
                        system.SetResetFilePath(result[0]);
                        system.RequestReset();
                    }
                }

                if (ImGui::MenuItem("Load Installed")) {
                    all_installed = GetInstalledList();
                }

                if (ImGui::MenuItem("Install CIA")) {
                    const std::vector<std::string> files =
                        pfd::open_file("Install CIA", *asl::Process::myDir(),
                                       {"CTR Importable Archive", "*.cia *.CIA"},
                                       pfd::opt::multiselect)
                            .result();

                    if (files.empty()) {
                        return;
                    }

                    std::shared_ptr<Service::AM::Module> am = Service::AM::GetModule(system);

                    std::atomic<bool> installing{true};
                    std::mutex mutex;
                    std::string current_file;
                    std::size_t current_file_current = 0;
                    std::size_t current_file_total = 0;

                    std::thread([&] {
                        for (const auto& file : files) {
                            {
                                std::lock_guard<std::mutex> lock(mutex);
                                current_file = file;
                            }

                            const Service::AM::InstallStatus status = Service::AM::InstallCIA(
                                file, [&](std::size_t current, std::size_t total) {
                                    std::lock_guard<std::mutex> lock(mutex);
                                    current_file_current = current;
                                    current_file_total = total;
                                });

                            switch (status) {
                            case Service::AM::InstallStatus::Success:
                                if (am != nullptr) {
                                    am->ScanForAllTitles();
                                }
                                break;
                            case Service::AM::InstallStatus::ErrorFailedToOpenFile:
                                pfd::message("vvctre", fmt::format("Failed to open {}", file),
                                             pfd::choice::ok, pfd::icon::error);
                                break;
                            case Service::AM::InstallStatus::ErrorFileNotFound:
                                pfd::message("vvctre", fmt::format("{} not found", file),
                                             pfd::choice::ok, pfd::icon::error);
                                break;
                            case Service::AM::InstallStatus::ErrorAborted:
                                pfd::message("vvctre", fmt::format("{} installation aborted", file),
                                             pfd::choice::ok, pfd::icon::error);
                                break;
                            case Service::AM::InstallStatus::ErrorInvalid:
                                pfd::message("vvctre", fmt::format("{} is invalid", file),
                                             pfd::choice::ok, pfd::icon::error);
                                break;
                            case Service::AM::InstallStatus::ErrorEncrypted:
                                pfd::message("vvctre", fmt::format("{} is encrypted", file),
                                             pfd::choice::ok, pfd::icon::error);
                                break;
                            }
                        }

                        installing = false;
                    }).detach();

                    SDL_Event event;

                    while (installing) {
                        while (SDL_PollEvent(&event)) {
                            ImGui_ImplSDL2_ProcessEvent(&event);

                            if (event.type == SDL_QUIT) {
                                if (pfd::message("vvctre", "Would you like to exit now?",
                                                 pfd::choice::yes_no, pfd::icon::question)
                                        .result() == pfd::button::yes) {
                                    vvctreShutdown(&plugin_manager);
                                    std::exit(0);
                                }
                            }
                        }

                        ImGui_ImplOpenGL3_NewFrame();
                        ImGui_ImplSDL2_NewFrame(window);
                        ImGui::NewFrame();

                        ImGui::OpenPopup("Installing CIA");
                        ImGui::SetNextWindowPos(
                            ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                        if (ImGui::BeginPopupModal("Installing CIA", nullptr,
                                                   ImGuiWindowFlags_NoSavedSettings |
                                                       ImGuiWindowFlags_NoMove |
                                                       ImGuiWindowFlags_AlwaysAutoResize)) {
                            std::lock_guard<std::mutex> lock(mutex);
                            ImGui::PushTextWrapPos(io.DisplaySize.x * 0.9f);
                            ImGui::Text("Installing %s", current_file.c_str());
                            ImGui::PopTextWrapPos();
                            ImGui::ProgressBar(static_cast<float>(current_file_current) /
                                               static_cast<float>(current_file_total));
                            ImGui::EndPopup();
                        }

                        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                        glClear(GL_COLOR_BUFFER_BIT);
                        ImGui::Render();
                        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                        SDL_GL_SwapWindow(window);
                    }

                    return;
                }

                if (ImGui::BeginMenu("Amiibo")) {
                    if (ImGui::MenuItem("Load File (Encrypted)")) {
                        const std::vector<std::string> result =
                            pfd::open_file("Load Amiibo", *asl::Process::myDir(),
                                           {"Amiibo Files", "*.bin *.BIN"})
                                .result();

                        if (!result.empty()) {
                            FileUtil::IOFile file(result[0], "rb");
                            Service::NFC::AmiiboData data;
                            if (file.ReadArray(&data, 1) == 1) {
                                std::shared_ptr<Service::NFC::Module::Interface> nfc =
                                    system.ServiceManager()
                                        .GetService<Service::NFC::Module::Interface>("nfc:u");
                                if (nfc != nullptr) {
                                    nfc->LoadAmiibo(data);
                                }
                            } else {
                                pfd::message("vvctre", "Failed to load the amiibo file",
                                             pfd::choice::ok, pfd::icon::error);
                            }
                        }
                    }

                    if (ImGui::MenuItem("Load File (Decrypted)")) {
                        const std::vector<std::string> result =
                            pfd::open_file("Load Amiibo", *asl::Process::myDir(),
                                           {"Amiibo Files", "*.bin *.BIN"})
                                .result();

                        if (!result.empty()) {
                            FileUtil::IOFile file(result[0], "rb");
                            std::array<u8, 540> array;
                            if (file.ReadBytes(array.data(), 540) == 540) {
                                std::shared_ptr<Service::NFC::Module::Interface> nfc =
                                    system.ServiceManager()
                                        .GetService<Service::NFC::Module::Interface>("nfc:u");
                                if (nfc != nullptr) {
                                    Service::NFC::AmiiboData data{};
                                    std::memcpy(&data.uuid, &array[0x1D4], data.uuid.size());
                                    std::memcpy(&data.char_id, &array[0x1DC], 8);
                                    nfc->LoadAmiibo(data);
                                }
                            } else {
                                pfd::message("vvctre", "Failed to load the amiibo file",
                                             pfd::choice::ok, pfd::icon::error);
                            }
                        }
                    }

                    if (ImGui::BeginMenu("Generate & Load")) {
                        if (ImGui::InputTextWithHint("##custom_id", "Custom ID",
                                                     &amiibo_generate_and_load_custom_id,
                                                     ImGuiInputTextFlags_EnterReturnsTrue) &&
                            !amiibo_generate_and_load_custom_id.empty()) {
                            std::shared_ptr<Service::NFC::Module::Interface> nfc =
                                system.ServiceManager().GetService<Service::NFC::Module::Interface>(
                                    "nfc:u");
                            if (nfc != nullptr) {
                                try {
                                    const u64 id = Common::swap64(std::stoull(
                                        amiibo_generate_and_load_custom_id, nullptr, 0));
                                    Service::NFC::AmiiboData data{};
                                    CryptoPP::AutoSeededRandomPool rng;
                                    rng.GenerateBlock(
                                        static_cast<CryptoPP::byte*>(data.uuid.data()),
                                        data.uuid.size());
                                    std::memcpy(&data.char_id, &id, sizeof(id));
                                    nfc->LoadAmiibo(data);
                                } catch (const std::invalid_argument&) {
                                }
                            }
                        }

                        if (ImGui::InputTextWithHint("##search", "Search",
                                                     &amiibo_generate_and_load_search_text_,
                                                     ImGuiInputTextFlags_EnterReturnsTrue)) {
                            amiibo_generate_and_load_search_text =
                                amiibo_generate_and_load_search_text_;
                            amiibo_generate_and_load_search_results.clear();

                            asl::String lower_case_text =
                                asl::String(amiibo_generate_and_load_search_text.c_str())
                                    .toLowerCase();

                            if (!amiibo_generate_and_load_search_text.empty()) {
                                for (const auto& a : amiibos) {
                                    const auto [id, name] = a;
                                    if (asl::String(fmt::format("{} - {} (0x{:016X})",
                                                                amiibo_series.at((id >> 8) & 0xFF),
                                                                name, id)
                                                        .c_str())
                                            .toLowerCase()
                                            .contains(lower_case_text)) {
                                        amiibo_generate_and_load_search_results.push_back(a);
                                    }
                                }
                            }
                        }

                        const auto& v = amiibo_generate_and_load_search_text.empty()
                                            ? amiibos
                                            : amiibo_generate_and_load_search_results;

                        ImGuiListClipper clipper(v.size());

                        while (clipper.Step()) {
                            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                                const auto [id, name] = v[i];

                                if (ImGui::MenuItem(fmt::format("{} - {} (0x{:016X})",
                                                                amiibo_series.at((id >> 8) & 0xFF),
                                                                name, id)
                                                        .c_str())) {
                                    std::shared_ptr<Service::NFC::Module::Interface> nfc =
                                        system.ServiceManager()
                                            .GetService<Service::NFC::Module::Interface>("nfc:u");
                                    if (nfc != nullptr) {
                                        Service::NFC::AmiiboData data{};
                                        CryptoPP::AutoSeededRandomPool rng;
                                        rng.GenerateBlock(
                                            static_cast<CryptoPP::byte*>(data.uuid.data()),
                                            data.uuid.size());
                                        const u64 swapped_id = Common::swap64(id);
                                        std::memcpy(&data.char_id, &swapped_id, sizeof(swapped_id));
                                        nfc->LoadAmiibo(data);
                                    }
                                }
                            }
                        }

                        ImGui::EndMenu();
                    }

                    if (ImGui::MenuItem("Remove")) {
                        std::shared_ptr<Service::NFC::Module::Interface> nfc =
                            system.ServiceManager().GetService<Service::NFC::Module::Interface>(
                                "nfc:u");
                        if (nfc != nullptr) {
                            nfc->RemoveAmiibo();
                        }
                    }

                    if (ImGui::MenuItem("Save (Encrypted)")) {
                        const std::string path = pfd::save_file("Save Amiibo", "amiibo.bin",
                                                                {"Amiibo Files", "*.bin *.BIN"})
                                                     .result();

                        if (!path.empty()) {
                            FileUtil::IOFile file(path, "wb");

                            std::shared_ptr<Service::NFC::Module::Interface> nfc =
                                system.ServiceManager().GetService<Service::NFC::Module::Interface>(
                                    "nfc:u");
                            if (nfc != nullptr) {
                                file.WriteObject(nfc->GetAmiiboData());
                            }
                        }
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Settings")) {
                if (ImGui::BeginMenu("General")) {
                    if (ImGui::Checkbox("Use CPU JIT", &Settings::values.use_cpu_jit)) {
                        request_reset = true;
                    }

                    ImGui::PushTextWrapPos();
                    ImGui::TextUnformatted("If you enable or disable the CPU JIT, emulation will "
                                           "restart when the menu is closed.");
                    ImGui::PopTextWrapPos();

                    ImGui::NewLine();

                    ImGui::Checkbox("Limit Speed", &Settings::values.limit_speed);

                    ImGui::Checkbox("Enable Custom CPU Ticks",
                                    &Settings::values.use_custom_cpu_ticks);

                    if (Settings::values.limit_speed) {
                        ImGui::InputScalar("Speed Limit", ImGuiDataType_U16,
                                           &Settings::values.speed_limit, nullptr, nullptr, "%d%%");
                    }

                    if (Settings::values.use_custom_cpu_ticks) {
                        ImGui::InputScalar("Custom CPU Ticks", ImGuiDataType_U64,
                                           &Settings::values.custom_cpu_ticks);
                    }

                    u32 min = 5;
                    u32 max = 400;
                    ImGui::SliderScalar("CPU Clock Percentage", ImGuiDataType_U32,
                                        &Settings::values.cpu_clock_percentage, &min, &max, "%d%%");

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Audio")) {
                    ImGui::TextUnformatted("DSP LLE");
                    ImGui::Separator();

                    if (ImGui::Checkbox("Enable", &Settings::values.enable_dsp_lle)) {
                        request_reset = true;
                    }

                    if (Settings::values.enable_dsp_lle) {
                        if (ImGui::Checkbox("Use Multiple Threads",
                                            &Settings::values.enable_dsp_lle_multithread)) {
                            request_reset = true;
                        }
                    }

                    ImGui::NewLine();

                    ImGui::PushTextWrapPos();
                    ImGui::TextUnformatted("If you change anything here, emulation will "
                                           "restart when the menu is closed.");
                    ImGui::PopTextWrapPos();

                    ImGui::NewLine();

                    ImGui::TextUnformatted("Output");
                    ImGui::Separator();

                    if (ImGui::Checkbox("Enable Stretching##Output",
                                        &Settings::values.enable_audio_stretching)) {
                        Settings::Apply();
                    }

                    ImGui::SliderFloat("Volume##Output", &Settings::values.audio_volume, 0.0f,
                                       1.0f);

                    if (ImGui::BeginCombo("Sink##Output", Settings::values.audio_sink_id.c_str())) {
                        if (ImGui::Selectable("auto")) {
                            Settings::values.audio_sink_id = "auto";
                            Settings::Apply();
                        }
                        for (const auto& sink : AudioCore::GetSinkIDs()) {
                            if (ImGui::Selectable(sink)) {
                                Settings::values.audio_sink_id = sink;
                                Settings::Apply();
                            }
                        }
                        ImGui::EndCombo();
                    }

                    if (ImGui::BeginCombo("Device##Output",
                                          Settings::values.audio_device_id.c_str())) {
                        if (ImGui::Selectable("auto")) {
                            Settings::values.audio_device_id = "auto";
                            Settings::Apply();
                        }

                        for (const auto& device :
                             AudioCore::GetDeviceListForSink(Settings::values.audio_sink_id)) {
                            if (ImGui::Selectable(device.c_str())) {
                                Settings::values.audio_device_id = device;
                                Settings::Apply();
                            }
                        }

                        ImGui::EndCombo();
                    }

                    ImGui::NewLine();

                    ImGui::TextUnformatted("Microphone");
                    ImGui::Separator();

                    if (ImGui::BeginCombo("Source##Microphone", [] {
                            switch (Settings::values.microphone_input_type) {
                            case Settings::MicrophoneInputType::None:
                                return "Disabled";
                            case Settings::MicrophoneInputType::Real:
                                return "Real Device";
                            case Settings::MicrophoneInputType::Static:
                                return "Static Noise";
                            default:
                                break;
                            }

                            return "Invalid";
                        }())) {
                        if (ImGui::Selectable("Disabled")) {
                            Settings::values.microphone_input_type =
                                Settings::MicrophoneInputType::None;
                            Settings::Apply();
                        }
                        if (ImGui::Selectable("Real Device")) {
                            Settings::values.microphone_input_type =
                                Settings::MicrophoneInputType::Real;
                            Settings::Apply();
                        }
                        if (ImGui::Selectable("Static Noise")) {
                            Settings::values.microphone_input_type =
                                Settings::MicrophoneInputType::Static;
                            Settings::Apply();
                        }
                        ImGui::EndCombo();
                    }

                    if (Settings::values.microphone_input_type ==
                        Settings::MicrophoneInputType::Real) {
                        if (ImGui::BeginCombo("Device##Microphone",
                                              Settings::values.microphone_device.c_str())) {
                            if (ImGui::Selectable("auto")) {
                                Settings::values.microphone_device = "auto";
                                Settings::Apply();
                            }
#ifdef HAVE_CUBEB
                            for (const auto& device : AudioCore::ListCubebInputDevices()) {
                                if (ImGui::Selectable(device.c_str())) {
                                    Settings::values.microphone_device = device;
                                    Settings::Apply();
                                }
                            }
#endif

                            ImGui::EndCombo();
                        }
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Camera")) {
                    ImGui::TextUnformatted("Inner");
                    ImGui::Separator();

                    if (ImGui::BeginCombo("Engine##Inner",
                                          Settings::values
                                              .camera_engine[static_cast<std::size_t>(
                                                  Service::CAM::CameraIndex::InnerCamera)]
                                              .c_str())) {
                        if (ImGui::Selectable("blank")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)] = "blank";
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        if (ImGui::Selectable("image (parameter: file path or URL)")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::InnerCamera)] = "image";
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if (Settings::values.camera_engine[static_cast<std::size_t>(
                            Service::CAM::CameraIndex::InnerCamera)] == "image") {
                        if (GUI_CameraAddBrowse(
                                "...##Inner",
                                static_cast<std::size_t>(Service::CAM::CameraIndex::InnerCamera))) {
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        if (ImGui::InputText(
                                "Parameter##Inner",
                                &Settings::values.camera_parameter[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)])) {
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        if (ImGui::BeginCombo("Flip##Inner", [] {
                                switch (Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)]) {
                                case Service::CAM::Flip::None:
                                    return "None";
                                case Service::CAM::Flip::Horizontal:
                                    return "Horizontal";
                                case Service::CAM::Flip::Vertical:
                                    return "Vertical";
                                case Service::CAM::Flip::Reverse:
                                    return "Reverse";
                                default:
                                    return "Invalid";
                                }
                            }())) {
                            if (ImGui::Selectable("None")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)] =
                                    Service::CAM::Flip::None;

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            if (ImGui::Selectable("Horizontal")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)] =
                                    Service::CAM::Flip::Horizontal;

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            if (ImGui::Selectable("Vertical")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)] =
                                    Service::CAM::Flip::Vertical;

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            if (ImGui::Selectable("Reverse")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::InnerCamera)] =
                                    Service::CAM::Flip::Reverse;

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }

                    ImGui::NewLine();

                    ImGui::TextUnformatted("Outer Left");
                    ImGui::Separator();

                    if (ImGui::BeginCombo("Engine##Outer Left",
                                          Settings::values
                                              .camera_engine[static_cast<std::size_t>(
                                                  Service::CAM::CameraIndex::OuterLeftCamera)]
                                              .c_str())) {
                        if (ImGui::Selectable("blank")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)] = "blank";
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        if (ImGui::Selectable("image (parameter: file path or URL)")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterLeftCamera)] = "image";
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if (Settings::values.camera_engine[static_cast<std::size_t>(
                            Service::CAM::CameraIndex::OuterLeftCamera)] == "image") {
                        if (GUI_CameraAddBrowse("...##Outer Left",
                                                static_cast<std::size_t>(
                                                    Service::CAM::CameraIndex::OuterLeftCamera))) {
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        if (ImGui::InputText(
                                "Parameter##Outer Left",
                                &Settings::values.camera_parameter[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)])) {
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        if (ImGui::BeginCombo("Flip##Outer Left", [] {
                                switch (Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)]) {
                                case Service::CAM::Flip::None:
                                    return "None";
                                case Service::CAM::Flip::Horizontal:
                                    return "Horizontal";
                                case Service::CAM::Flip::Vertical:
                                    return "Vertical";
                                case Service::CAM::Flip::Reverse:
                                    return "Reverse";
                                default:
                                    return "Invalid";
                                }
                            }())) {
                            if (ImGui::Selectable("None")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] =
                                    Service::CAM::Flip::None;

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            if (ImGui::Selectable("Horizontal")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] =
                                    Service::CAM::Flip::Horizontal;

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            if (ImGui::Selectable("Vertical")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] =
                                    Service::CAM::Flip::Vertical;

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            if (ImGui::Selectable("Reverse")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterLeftCamera)] =
                                    Service::CAM::Flip::Reverse;

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }

                    ImGui::NewLine();

                    ImGui::TextUnformatted("Outer Right");
                    ImGui::Separator();

                    if (ImGui::BeginCombo("Engine##Outer Right",
                                          Settings::values
                                              .camera_engine[static_cast<std::size_t>(
                                                  Service::CAM::CameraIndex::OuterRightCamera)]
                                              .c_str())) {
                        if (ImGui::Selectable("blank")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)] = "blank";
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        if (ImGui::Selectable("image (parameter: file path or URL)")) {
                            Settings::values.camera_engine[static_cast<std::size_t>(
                                Service::CAM::CameraIndex::OuterRightCamera)] = "image";
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if (Settings::values.camera_engine[static_cast<std::size_t>(
                            Service::CAM::CameraIndex::OuterRightCamera)] == "image") {
                        if (GUI_CameraAddBrowse("...##Outer Right",
                                                static_cast<std::size_t>(
                                                    Service::CAM::CameraIndex::OuterRightCamera))) {
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        if (ImGui::InputText(
                                "Parameter##Outer Right",
                                &Settings::values.camera_parameter[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)])) {
                            std::shared_ptr<Service::CAM::Module> cam =
                                Service::CAM::GetModule(system);
                            if (cam != nullptr) {
                                cam->ReloadCameraDevices();
                            }
                        }
                        if (ImGui::BeginCombo("Flip##Outer Right", [] {
                                switch (Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)]) {
                                case Service::CAM::Flip::None:
                                    return "None";
                                case Service::CAM::Flip::Horizontal:
                                    return "Horizontal";
                                case Service::CAM::Flip::Vertical:
                                    return "Vertical";
                                case Service::CAM::Flip::Reverse:
                                    return "Reverse";
                                default:
                                    return "Invalid";
                                }
                            }())) {
                            if (ImGui::Selectable("None")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] =
                                    Service::CAM::Flip::None;

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            if (ImGui::Selectable("Horizontal")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] =
                                    Service::CAM::Flip::Horizontal;

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            if (ImGui::Selectable("Vertical")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] =
                                    Service::CAM::Flip::Vertical;

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            if (ImGui::Selectable("Reverse")) {
                                Settings::values.camera_flip[static_cast<std::size_t>(
                                    Service::CAM::CameraIndex::OuterRightCamera)] =
                                    Service::CAM::Flip::Reverse;

                                std::shared_ptr<Service::CAM::Module> cam =
                                    Service::CAM::GetModule(system);
                                if (cam != nullptr) {
                                    cam->ReloadCameraDevices();
                                }
                            }
                            ImGui::EndCombo();
                        }
                    }
                    ImGui::Unindent();

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("System")) {
                    std::shared_ptr<Service::CFG::Module> cfg = Service::CFG::GetModule(system);

                    if (cfg != nullptr) {
                        ImGui::PushTextWrapPos();
                        ImGui::TextUnformatted("If you change anything here, emulation will "
                                               "restart when the menu is closed.");
                        ImGui::PopTextWrapPos();
                        ImGui::NewLine();

                        ImGui::TextUnformatted("Config Savegame");
                        ImGui::Separator();

                        std::string username = Common::UTF16ToUTF8(cfg->GetUsername());
                        if (ImGui::InputText("Username", &username)) {
                            cfg->SetUsername(Common::UTF8ToUTF16(username));
                            config_savegame_changed = true;
                        }

                        auto [month, day] = cfg->GetBirthday();

                        if (ImGui::BeginCombo("Birthday Month", [&] {
                                switch (month) {
                                case 1:
                                    return "January";
                                case 2:
                                    return "February";
                                case 3:
                                    return "March";
                                case 4:
                                    return "April";
                                case 5:
                                    return "May";
                                case 6:
                                    return "June";
                                case 7:
                                    return "July";
                                case 8:
                                    return "August";
                                case 9:
                                    return "September";
                                case 10:
                                    return "October";
                                case 11:
                                    return "November";
                                case 12:
                                    return "December";
                                default:
                                    break;
                                }

                                return "Invalid";
                            }())) {
                            if (ImGui::Selectable("January")) {
                                cfg->SetBirthday(1, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("February")) {
                                cfg->SetBirthday(2, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("March")) {
                                cfg->SetBirthday(3, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("April")) {
                                cfg->SetBirthday(4, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("May")) {
                                cfg->SetBirthday(5, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("June")) {
                                cfg->SetBirthday(6, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("July")) {
                                cfg->SetBirthday(7, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("August")) {
                                cfg->SetBirthday(8, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("September")) {
                                cfg->SetBirthday(9, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("October")) {
                                cfg->SetBirthday(10, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("November")) {
                                cfg->SetBirthday(11, day);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("December")) {
                                cfg->SetBirthday(12, day);
                                config_savegame_changed = true;
                            }

                            ImGui::EndCombo();
                        }

                        if (ImGui::InputScalar("Birthday Day", ImGuiDataType_U8, &day)) {
                            cfg->SetBirthday(month, day);
                            config_savegame_changed = true;
                        }

                        if (ImGui::BeginCombo("Language", [&] {
                                switch (cfg->GetSystemLanguage()) {
                                case Service::CFG::SystemLanguage::LANGUAGE_JP:
                                    return "Japanese";
                                case Service::CFG::SystemLanguage::LANGUAGE_EN:
                                    return "English";
                                case Service::CFG::SystemLanguage::LANGUAGE_FR:
                                    return "French";
                                case Service::CFG::SystemLanguage::LANGUAGE_DE:
                                    return "German";
                                case Service::CFG::SystemLanguage::LANGUAGE_IT:
                                    return "Italian";
                                case Service::CFG::SystemLanguage::LANGUAGE_ES:
                                    return "Spanish";
                                case Service::CFG::SystemLanguage::LANGUAGE_ZH:
                                    return "Simplified Chinese";
                                case Service::CFG::SystemLanguage::LANGUAGE_KO:
                                    return "Korean";
                                case Service::CFG::SystemLanguage::LANGUAGE_NL:
                                    return "Dutch";
                                case Service::CFG::SystemLanguage::LANGUAGE_PT:
                                    return "Portuguese";
                                case Service::CFG::SystemLanguage::LANGUAGE_RU:
                                    return "Russian";
                                case Service::CFG::SystemLanguage::LANGUAGE_TW:
                                    return "Traditional Chinese";
                                default:
                                    break;
                                }

                                return "Invalid language";
                            }())) {
                            if (ImGui::Selectable("Japanese")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_JP);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("English")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_EN);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("French")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_FR);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("German")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_DE);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Italian")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_IT);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Spanish")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_ES);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Simplified Chinese")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_ZH);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Korean")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_KO);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Dutch")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_NL);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Portuguese")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_PT);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Russian")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_RU);
                                config_savegame_changed = true;
                            }

                            if (ImGui::Selectable("Traditional Chinese")) {
                                cfg->SetSystemLanguage(Service::CFG::SystemLanguage::LANGUAGE_TW);
                                config_savegame_changed = true;
                            }

                            ImGui::EndCombo();
                        }

                        if (ImGui::BeginCombo("Sound Output Mode", [&] {
                                switch (cfg->GetSoundOutputMode()) {
                                case Service::CFG::SoundOutputMode::SOUND_MONO:
                                    return "Mono";
                                case Service::CFG::SoundOutputMode::SOUND_STEREO:
                                    return "Stereo";
                                case Service::CFG::SoundOutputMode::SOUND_SURROUND:
                                    return "Surround";
                                default:
                                    break;
                                }

                                return "Invalid";
                            }())) {
                            if (ImGui::Selectable("Mono")) {
                                cfg->SetSoundOutputMode(Service::CFG::SoundOutputMode::SOUND_MONO);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Stereo")) {
                                cfg->SetSoundOutputMode(
                                    Service::CFG::SoundOutputMode::SOUND_STEREO);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Surround")) {
                                cfg->SetSoundOutputMode(
                                    Service::CFG::SoundOutputMode::SOUND_SURROUND);
                                config_savegame_changed = true;
                            }
                            ImGui::EndCombo();
                        }

                        if (ImGui::BeginCombo("Country", [&] {
                                switch (cfg->GetCountryCode()) {
                                case 1:
                                    return "Japan";
                                case 8:
                                    return "Anguilla";
                                case 9:
                                    return "Antigua and Barbuda";
                                case 10:
                                    return "Argentina";
                                case 11:
                                    return "Aruba";
                                case 12:
                                    return "Bahamas";
                                case 13:
                                    return "Barbados";
                                case 14:
                                    return "Belize";
                                case 15:
                                    return "Bolivia";
                                case 16:
                                    return "Brazil";
                                case 17:
                                    return "British Virgin Islands";
                                case 18:
                                    return "Canada";
                                case 19:
                                    return "Cayman Islands";
                                case 20:
                                    return "Chile";
                                case 21:
                                    return "Colombia";
                                case 22:
                                    return "Costa Rica";
                                case 23:
                                    return "Dominica";
                                case 24:
                                    return "Dominican Republic";
                                case 25:
                                    return "Ecuador";
                                case 26:
                                    return "El Salvador";
                                case 27:
                                    return "French Guiana";
                                case 28:
                                    return "Grenada";
                                case 29:
                                    return "Guadeloupe";
                                case 30:
                                    return "Guatemala";
                                case 31:
                                    return "Guyana";
                                case 32:
                                    return "Haiti";
                                case 33:
                                    return "Honduras";
                                case 34:
                                    return "Jamaica";
                                case 35:
                                    return "Martinique";
                                case 36:
                                    return "Mexico";
                                case 37:
                                    return "Montserrat";
                                case 38:
                                    return "Netherlands Antilles";
                                case 39:
                                    return "Nicaragua";
                                case 40:
                                    return "Panama";
                                case 41:
                                    return "Paraguay";
                                case 42:
                                    return "Peru";
                                case 43:
                                    return "Saint Kitts and Nevis";
                                case 44:
                                    return "Saint Lucia";
                                case 45:
                                    return "Saint Vincent and the Grenadines";
                                case 46:
                                    return "Suriname";
                                case 47:
                                    return "Trinidad and Tobago";
                                case 48:
                                    return "Turks and Caicos Islands";
                                case 49:
                                    return "United States";
                                case 50:
                                    return "Uruguay";
                                case 51:
                                    return "US Virgin Islands";
                                case 52:
                                    return "Venezuela";
                                case 64:
                                    return "Albania";
                                case 65:
                                    return "Australia";
                                case 66:
                                    return "Austria";
                                case 67:
                                    return "Belgium";
                                case 68:
                                    return "Bosnia and Herzegovina";
                                case 69:
                                    return "Botswana";
                                case 70:
                                    return "Bulgaria";
                                case 71:
                                    return "Croatia";
                                case 72:
                                    return "Cyprus";
                                case 73:
                                    return "Czech Republic";
                                case 74:
                                    return "Denmark";
                                case 75:
                                    return "Estonia";
                                case 76:
                                    return "Finland";
                                case 77:
                                    return "France";
                                case 78:
                                    return "Germany";
                                case 79:
                                    return "Greece";
                                case 80:
                                    return "Hungary";
                                case 81:
                                    return "Iceland";
                                case 82:
                                    return "Ireland";
                                case 83:
                                    return "Italy";
                                case 84:
                                    return "Latvia";
                                case 85:
                                    return "Lesotho";
                                case 86:
                                    return "Liechtenstein";
                                case 87:
                                    return "Lithuania";
                                case 88:
                                    return "Luxembourg";
                                case 89:
                                    return "Macedonia";
                                case 90:
                                    return "Malta";
                                case 91:
                                    return "Montenegro";
                                case 92:
                                    return "Mozambique";
                                case 93:
                                    return "Namibia";
                                case 94:
                                    return "Netherlands";
                                case 95:
                                    return "New Zealand";
                                case 96:
                                    return "Norway";
                                case 97:
                                    return "Poland";
                                case 98:
                                    return "Portugal";
                                case 99:
                                    return "Romania";
                                case 100:
                                    return "Russia";
                                case 101:
                                    return "Serbia";
                                case 102:
                                    return "Slovakia";
                                case 103:
                                    return "Slovenia";
                                case 104:
                                    return "South Africa";
                                case 105:
                                    return "Spain";
                                case 106:
                                    return "Swaziland";
                                case 107:
                                    return "Sweden";
                                case 108:
                                    return "Switzerland";
                                case 109:
                                    return "Turkey";
                                case 110:
                                    return "United Kingdom";
                                case 111:
                                    return "Zambia";
                                case 112:
                                    return "Zimbabwe";
                                case 113:
                                    return "Azerbaijan";
                                case 114:
                                    return "Mauritania";
                                case 115:
                                    return "Mali";
                                case 116:
                                    return "Niger";
                                case 117:
                                    return "Chad";
                                case 118:
                                    return "Sudan";
                                case 119:
                                    return "Eritrea";
                                case 120:
                                    return "Djibouti";
                                case 121:
                                    return "Somalia";
                                case 122:
                                    return "Andorra";
                                case 123:
                                    return "Gibraltar";
                                case 124:
                                    return "Guernsey";
                                case 125:
                                    return "Isle of Man";
                                case 126:
                                    return "Jersey";
                                case 127:
                                    return "Monaco";
                                case 128:
                                    return "Taiwan";
                                case 136:
                                    return "South Korea";
                                case 144:
                                    return "Hong Kong";
                                case 145:
                                    return "Macau";
                                case 152:
                                    return "Indonesia";
                                case 153:
                                    return "Singapore";
                                case 154:
                                    return "Thailand";
                                case 155:
                                    return "Philippines";
                                case 156:
                                    return "Malaysia";
                                case 160:
                                    return "China";
                                case 168:
                                    return "United Arab Emirates";
                                case 169:
                                    return "India";
                                case 170:
                                    return "Egypt";
                                case 171:
                                    return "Oman";
                                case 172:
                                    return "Qatar";
                                case 173:
                                    return "Kuwait";
                                case 174:
                                    return "Saudi Arabia";
                                case 175:
                                    return "Syria";
                                case 176:
                                    return "Bahrain";
                                case 177:
                                    return "Jordan";
                                case 184:
                                    return "San Marino";
                                case 185:
                                    return "Vatican City";
                                case 186:
                                    return "Bermuda";
                                default:
                                    break;
                                }

                                return "Invalid";
                            }())) {
                            if (ImGui::Selectable("Japan")) {
                                cfg->SetCountry(1);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Anguilla")) {
                                cfg->SetCountry(8);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Antigua and Barbuda")) {
                                cfg->SetCountry(9);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Argentina")) {
                                cfg->SetCountry(10);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Aruba")) {
                                cfg->SetCountry(11);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Bahamas")) {
                                cfg->SetCountry(12);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Barbados")) {
                                cfg->SetCountry(13);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Belize")) {
                                cfg->SetCountry(14);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Bolivia")) {
                                cfg->SetCountry(15);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Brazil")) {
                                cfg->SetCountry(16);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("British Virgin Islands")) {
                                cfg->SetCountry(17);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Canada")) {
                                cfg->SetCountry(18);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Cayman Islands")) {
                                cfg->SetCountry(19);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Chile")) {
                                cfg->SetCountry(20);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Colombia")) {
                                cfg->SetCountry(21);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Costa Rica")) {
                                cfg->SetCountry(22);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Dominica")) {
                                cfg->SetCountry(23);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Dominican Republic")) {
                                cfg->SetCountry(24);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Ecuador")) {
                                cfg->SetCountry(25);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("El Salvador")) {
                                cfg->SetCountry(26);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("French Guiana")) {
                                cfg->SetCountry(27);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Grenada")) {
                                cfg->SetCountry(28);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Guadeloupe")) {
                                cfg->SetCountry(29);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Guatemala")) {
                                cfg->SetCountry(30);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Guyana")) {
                                cfg->SetCountry(31);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Haiti")) {
                                cfg->SetCountry(32);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Honduras")) {
                                cfg->SetCountry(33);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Jamaica")) {
                                cfg->SetCountry(34);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Martinique")) {
                                cfg->SetCountry(35);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Mexico")) {
                                cfg->SetCountry(36);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Montserrat")) {
                                cfg->SetCountry(37);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Netherlands Antilles")) {
                                cfg->SetCountry(38);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Nicaragua")) {
                                cfg->SetCountry(39);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Panama")) {
                                cfg->SetCountry(40);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Paraguay")) {
                                cfg->SetCountry(41);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Peru")) {
                                cfg->SetCountry(42);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Saint Kitts and Nevis")) {
                                cfg->SetCountry(43);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Saint Lucia")) {
                                cfg->SetCountry(44);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Saint Vincent and the Grenadines")) {
                                cfg->SetCountry(45);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Suriname")) {
                                cfg->SetCountry(46);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Trinidad and Tobago")) {
                                cfg->SetCountry(47);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Turks and Caicos Islands")) {
                                cfg->SetCountry(48);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("United States")) {
                                cfg->SetCountry(49);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Uruguay")) {
                                cfg->SetCountry(50);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("US Virgin Islands")) {
                                cfg->SetCountry(51);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Venezuela")) {
                                cfg->SetCountry(52);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Albania")) {
                                cfg->SetCountry(64);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Australia")) {
                                cfg->SetCountry(65);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Austria")) {
                                cfg->SetCountry(66);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Belgium")) {
                                cfg->SetCountry(67);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Bosnia and Herzegovina")) {
                                cfg->SetCountry(68);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Botswana")) {
                                cfg->SetCountry(69);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Bulgaria")) {
                                cfg->SetCountry(70);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Croatia")) {
                                cfg->SetCountry(71);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Cyprus")) {
                                cfg->SetCountry(72);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Czech Republic")) {
                                cfg->SetCountry(73);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Denmark")) {
                                cfg->SetCountry(74);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Estonia")) {
                                cfg->SetCountry(75);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Finland")) {
                                cfg->SetCountry(76);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("France")) {
                                cfg->SetCountry(77);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Germany")) {
                                cfg->SetCountry(78);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Greece")) {
                                cfg->SetCountry(79);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Hungary")) {
                                cfg->SetCountry(80);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Iceland")) {
                                cfg->SetCountry(81);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Ireland")) {
                                cfg->SetCountry(82);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Italy")) {
                                cfg->SetCountry(83);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Latvia")) {
                                cfg->SetCountry(84);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Lesotho")) {
                                cfg->SetCountry(85);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Liechtenstein")) {
                                cfg->SetCountry(86);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Lithuania")) {
                                cfg->SetCountry(87);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Luxembourg")) {
                                cfg->SetCountry(88);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Macedonia")) {
                                cfg->SetCountry(89);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Malta")) {
                                cfg->SetCountry(90);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Montenegro")) {
                                cfg->SetCountry(91);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Mozambique")) {
                                cfg->SetCountry(92);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Namibia")) {
                                cfg->SetCountry(93);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Netherlands")) {
                                cfg->SetCountry(94);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("New Zealand")) {
                                cfg->SetCountry(95);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Norway")) {
                                cfg->SetCountry(96);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Poland")) {
                                cfg->SetCountry(97);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Portugal")) {
                                cfg->SetCountry(98);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Romania")) {
                                cfg->SetCountry(99);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Russia")) {
                                cfg->SetCountry(100);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Serbia")) {
                                cfg->SetCountry(101);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Slovakia")) {
                                cfg->SetCountry(102);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Slovenia")) {
                                cfg->SetCountry(103);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("South Africa")) {
                                cfg->SetCountry(104);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Spain")) {
                                cfg->SetCountry(105);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Swaziland")) {
                                cfg->SetCountry(106);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Sweden")) {
                                cfg->SetCountry(107);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Switzerland")) {
                                cfg->SetCountry(108);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Turkey")) {
                                cfg->SetCountry(109);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("United Kingdom")) {
                                cfg->SetCountry(110);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Zambia")) {
                                cfg->SetCountry(111);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Zimbabwe")) {
                                cfg->SetCountry(112);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Azerbaijan")) {
                                cfg->SetCountry(113);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Mauritania")) {
                                cfg->SetCountry(114);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Mali")) {
                                cfg->SetCountry(115);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Niger")) {
                                cfg->SetCountry(116);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Chad")) {
                                cfg->SetCountry(117);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Sudan")) {
                                cfg->SetCountry(118);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Eritrea")) {
                                cfg->SetCountry(119);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Djibouti")) {
                                cfg->SetCountry(120);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Somalia")) {
                                cfg->SetCountry(121);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Andorra")) {
                                cfg->SetCountry(122);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Gibraltar")) {
                                cfg->SetCountry(123);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Guernsey")) {
                                cfg->SetCountry(124);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Isle of Man")) {
                                cfg->SetCountry(125);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Jersey")) {
                                cfg->SetCountry(126);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Monaco")) {
                                cfg->SetCountry(127);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Taiwan")) {
                                cfg->SetCountry(128);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("South Korea")) {
                                cfg->SetCountry(136);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Hong Kong")) {
                                cfg->SetCountry(144);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Macau")) {
                                cfg->SetCountry(145);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Indonesia")) {
                                cfg->SetCountry(152);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Singapore")) {
                                cfg->SetCountry(153);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Thailand")) {
                                cfg->SetCountry(154);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Philippines")) {
                                cfg->SetCountry(155);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Malaysia")) {
                                cfg->SetCountry(156);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("China")) {
                                cfg->SetCountry(160);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("United Arab Emirates")) {
                                cfg->SetCountry(168);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("India")) {
                                cfg->SetCountry(169);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Egypt")) {
                                cfg->SetCountry(170);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Oman")) {
                                cfg->SetCountry(171);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Qatar")) {
                                cfg->SetCountry(172);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Kuwait")) {
                                cfg->SetCountry(173);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Saudi Arabia")) {
                                cfg->SetCountry(174);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Syria")) {
                                cfg->SetCountry(175);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Bahrain")) {
                                cfg->SetCountry(176);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Jordan")) {
                                cfg->SetCountry(177);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("San Marino")) {
                                cfg->SetCountry(184);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Vatican City")) {
                                cfg->SetCountry(185);
                                config_savegame_changed = true;
                            }
                            if (ImGui::Selectable("Bermuda")) {
                                cfg->SetCountry(186);
                                config_savegame_changed = true;
                            }
                            ImGui::EndCombo();
                        }

                        if (ImGui::Button("Regenerate Console ID")) {
                            u32 random_number;
                            u64 console_id;
                            cfg->GenerateConsoleUniqueId(random_number, console_id);
                            cfg->SetConsoleUniqueId(random_number, console_id);
                            config_savegame_changed = true;
                        }

                        if (ImGui::BeginPopupContextItem("Console ID",
                                                         ImGuiPopupFlags_MouseButtonRight)) {
                            std::string console_id =
                                fmt::format("0x{:016X}", cfg->GetConsoleUniqueId());
                            ImGui::InputText("##Console ID", &console_id[0], 18,
                                             ImGuiInputTextFlags_ReadOnly);
                            ImGui::EndPopup();
                        }

                        ImGui::NewLine();
                    }

                    ImGui::TextUnformatted("Play Coins");
                    ImGui::Separator();
                    const u16 min = 0;
                    const u16 max = 300;
                    if (ImGui::IsWindowAppearing()) {
                        play_coins = Service::PTM::Module::GetPlayCoins();
                    }
                    if (ImGui::SliderScalar("Play Coins", ImGuiDataType_U16, &play_coins, &min,
                                            &max)) {
                        play_coins_changed = true;
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Graphics")) {
                    if (ImGui::Checkbox("Use Hardware Renderer",
                                        &Settings::values.use_hardware_renderer)) {
                        Settings::Apply();
                    }

                    if (Settings::values.use_hardware_renderer) {
                        ImGui::Indent();

                        if (ImGui::Checkbox("Use Hardware Shader",
                                            &Settings::values.use_hardware_shader)) {
                            Settings::Apply();
                        }

                        if (Settings::values.use_hardware_shader) {
                            ImGui::Indent();

                            if (ImGui::Checkbox(
                                    "Accurate Multiplication",
                                    &Settings::values.hardware_shader_accurate_multiplication)) {
                                Settings::Apply();
                            }

                            ImGui::Unindent();
                        }

                        ImGui::Unindent();
                    }

                    ImGui::Checkbox("Use Shader JIT", &Settings::values.use_shader_jit);
                    ImGui::Checkbox("Enable VSync", &Settings::values.enable_vsync);

                    if (ImGui::Checkbox("Enable Linear Filtering",
                                        &Settings::values.enable_linear_filtering)) {
                        Settings::Apply();
                    }

                    if (ImGui::Checkbox("Sharper Distant Objects",
                                        &Settings::values.sharper_distant_objects)) {
                        request_reset = true;
                    }

                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::PushTextWrapPos(io.DisplaySize.x * 0.5f);
                        ImGui::TextUnformatted(
                            "If you change this, emulation will restart when the menu is closed");
                        ImGui::PopTextWrapPos();
                        ImGui::EndTooltip();
                    }

                    ImGui::Checkbox("Dump Textures", &Settings::values.dump_textures);
                    ImGui::Checkbox("Use Custom Textures", &Settings::values.custom_textures);
                    ImGui::Checkbox("Preload Custom Textures", &Settings::values.preload_textures);

                    if (ImGui::ColorEdit3("Background Color",
                                          &Settings::values.background_color_red,
                                          ImGuiColorEditFlags_NoInputs)) {
                        VideoCore::g_renderer_background_color_update_requested = true;
                    }

                    const u16 min = 0;
                    const u16 max = 10;
                    ImGui::SliderScalar("Resolution", ImGuiDataType_U16,
                                        &Settings::values.resolution, &min, &max,
                                        Settings::values.resolution == 0 ? "Window Size" : "%d");

                    ImGui::InputText("Post Processing Shader",
                                     &Settings::values.post_processing_shader);
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        VideoCore::g_renderer_shader_update_requested = true;
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted("File name without extension and folder");
                        ImGui::EndTooltip();
                    }

                    if (ImGui::BeginCombo("Texture Filter",
                                          Settings::values.texture_filter.c_str())) {
                        const auto& filters = OpenGL::TextureFilterer::GetFilterNames();

                        for (const auto& filter : filters) {
                            if (ImGui::Selectable(std::string(filter).c_str())) {
                                Settings::values.texture_filter = filter;
                                Settings::Apply();
                            }
                        }

                        ImGui::EndCombo();
                    }

                    if (ImGui::BeginCombo("3D Mode", [] {
                            switch (Settings::values.render_3d) {
                            case Settings::StereoRenderOption::Off:
                                return "Off";
                            case Settings::StereoRenderOption::SideBySide:
                                return "Side by Side";
                            case Settings::StereoRenderOption::Anaglyph:
                                return "Anaglyph";
                            case Settings::StereoRenderOption::Interlaced:
                                return "Interlaced";
                            default:
                                break;
                            }

                            return "Invalid value";
                        }())) {

                        if (ImGui::Selectable("Off", Settings::values.render_3d ==
                                                         Settings::StereoRenderOption::Off)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::Off;
                            Settings::values.post_processing_shader = "none (builtin)";
                            Settings::Apply();
                        }

                        if (ImGui::Selectable("Side by Side",
                                              Settings::values.render_3d ==
                                                  Settings::StereoRenderOption::SideBySide)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::SideBySide;
                            Settings::values.post_processing_shader = "none (builtin)";
                            Settings::Apply();
                        }

                        if (ImGui::Selectable("Anaglyph",
                                              Settings::values.render_3d ==
                                                  Settings::StereoRenderOption::Anaglyph)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::Anaglyph;
                            Settings::values.post_processing_shader = "dubois (builtin)";
                            Settings::Apply();
                        }

                        if (ImGui::Selectable("Interlaced",
                                              Settings::values.render_3d ==
                                                  Settings::StereoRenderOption::Interlaced)) {
                            Settings::values.render_3d = Settings::StereoRenderOption::Interlaced;
                            Settings::values.post_processing_shader = "horizontal (builtin)";
                            Settings::Apply();
                        }

                        ImGui::EndCombo();
                    }

                    u8 factor_3d = Settings::values.factor_3d;
                    const u8 factor_3d_min = 0;
                    const u8 factor_3d_max = 100;
                    if (ImGui::SliderScalar("3D Factor", ImGuiDataType_U8, &factor_3d,
                                            &factor_3d_min, &factor_3d_max, "%d%%")) {
                        Settings::values.factor_3d = factor_3d;
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Controls")) {
                    GUI_AddControlsSettings(is_open, &system, plugin_manager, io);

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("LLE Modules")) {
                    ImGui::PushTextWrapPos(io.DisplaySize.x * 0.5f);
                    ImGui::TextUnformatted("If you enable or disable a LLE module, emulation will "
                                           "restart when the menu is closed.");
                    ImGui::PopTextWrapPos();

                    for (auto& module : Settings::values.lle_modules) {
                        if (ImGui::Checkbox(module.first.c_str(), &module.second)) {
                            request_reset = true;
                        }
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Hacks")) {
                    if (ImGui::Checkbox("Priority Boost",
                                        &Settings::values.enable_priority_boost)) {
                        request_reset = true;
                    }

                    ImGui::PushTextWrapPos(io.DisplaySize.x * 0.5f);
                    ImGui::TextUnformatted(
                        "If you enable or disable Priority Boost, emulation will "
                        "restart when the menu is closed.");
                    ImGui::PopTextWrapPos();

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("GUI")) {
                    ImGui::ColorEdit4("FPS Color", &fps_color.x,
                                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar |
                                          ImGuiColorEditFlags_AlphaPreview);

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                if (ImGui::BeginMenu("Layout")) {
                    if (!Settings::values.use_custom_layout) {
                        if (ImGui::BeginCombo("Layout", [] {
                                switch (Settings::values.layout) {
                                case Settings::Layout::Default:
                                    return "Default";
                                case Settings::Layout::SingleScreen:
                                    return "Single Screen";
                                case Settings::Layout::LargeScreen:
                                    return "Large Screen";
                                case Settings::Layout::SideScreen:
                                    return "Side by Side";
                                case Settings::Layout::MediumScreen:
                                    return "Medium Screen";
                                default:
                                    break;
                                }

                                return "Invalid";
                            }())) {
                            if (ImGui::Selectable("Default")) {
                                Settings::values.layout = Settings::Layout::Default;
                                VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                            }
                            if (ImGui::Selectable("Single Screen")) {
                                Settings::values.layout = Settings::Layout::SingleScreen;
                                VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                            }
                            if (ImGui::Selectable("Large Screen")) {
                                Settings::values.layout = Settings::Layout::LargeScreen;
                                VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                            }
                            if (ImGui::Selectable("Side by Side")) {
                                Settings::values.layout = Settings::Layout::SideScreen;
                                VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                            }
                            if (ImGui::Selectable("Medium Screen")) {
                                Settings::values.layout = Settings::Layout::MediumScreen;
                                VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                            }
                            ImGui::EndCombo();
                        }
                    }

                    if (ImGui::Checkbox("Use Custom Layout", &Settings::values.use_custom_layout)) {
                        VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                    }

                    if (ImGui::Checkbox("Swap Screens", &Settings::values.swap_screens)) {
                        VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                    }

                    if (ImGui::Checkbox("Upright Screens", &Settings::values.upright_screens)) {
                        VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                    }

                    if (Settings::values.use_custom_layout) {
                        ImGui::NewLine();

                        ImGui::TextUnformatted("Top Screen");
                        ImGui::Separator();

                        if (ImGui::InputScalar("Left##Top Screen", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_top_left)) {
                            VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                        }

                        if (ImGui::InputScalar("Top##Top Screen", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_top_top)) {
                            VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                        }

                        if (ImGui::InputScalar("Right##Top Screen", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_top_right)) {
                            VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                        }

                        if (ImGui::InputScalar("Bottom##Top Screen", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_top_bottom)) {
                            VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                        }

                        ImGui::NewLine();

                        ImGui::TextUnformatted("Bottom Screen");
                        ImGui::Separator();

                        if (ImGui::InputScalar("Left##Bottom Screen", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_bottom_left)) {
                            VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                        }

                        if (ImGui::InputScalar("Top##Bottom Screen", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_bottom_top)) {
                            VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                        }

                        if (ImGui::InputScalar("Right##Bottom Screen", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_bottom_right)) {
                            VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                        }

                        if (ImGui::InputScalar("Bottom##Bottom Screen", ImGuiDataType_U16,
                                               &Settings::values.custom_layout_bottom_bottom)) {
                            VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
                        }
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Debugging")) {
                    if (ImGui::Checkbox("IPC Recorder", &show_ipc_recorder_window)) {
                        if (!show_ipc_recorder_window) {
                            IPCDebugger::Recorder& r = system.Kernel().GetIPCRecorder();

                            r.SetEnabled(false);
                            r.UnbindCallback(ipc_recorder_callback);

                            all_ipc_records.clear();
                            ipc_recorder_search_results.clear();
                            ipc_recorder_search_text.clear();
                            ipc_recorder_callback = nullptr;
                        }
                    }

                    ImGui::EndMenu();
                }

                ImGui::Checkbox("Cheats", &show_cheats_window);

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Emulation")) {
                if (ImGui::MenuItem("Restart")) {
                    request_reset = true;
                }

                if (ImGui::MenuItem("Restart With Different Log Filter")) {
                    SDL_Event event;
                    std::string new_log_filter = Settings::values.log_filter;
                    bool new_log_filter_window_open = true;

                    while (is_open) {
                        while (SDL_PollEvent(&event)) {
                            ImGui_ImplSDL2_ProcessEvent(&event);

                            if (event.type == SDL_QUIT) {
                                if (pfd::message("vvctre", "Would you like to exit now?",
                                                 pfd::choice::yes_no, pfd::icon::question)
                                        .result() == pfd::button::yes) {
                                    vvctreShutdown(&plugin_manager);
                                    std::exit(0);
                                }
                            }
                        }

                        ImGui_ImplOpenGL3_NewFrame();
                        ImGui_ImplSDL2_NewFrame(window);
                        ImGui::NewFrame();

                        ImGui::OpenPopup("New Log Filter");
                        ImGui::SetNextWindowPos(
                            ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                        if (ImGui::BeginPopupModal("New Log Filter", &new_log_filter_window_open,
                                                   ImGuiWindowFlags_NoSavedSettings |
                                                       ImGuiWindowFlags_NoMove |
                                                       ImGuiWindowFlags_AlwaysAutoResize)) {
                            if (ImGui::InputText("##New Log Filter", &new_log_filter,
                                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                                Settings::values.log_filter = new_log_filter;
                                Log::Filter log_filter(Log::Level::Debug);
                                log_filter.ParseFilterString(Settings::values.log_filter);
                                Log::SetGlobalFilter(log_filter);
                                request_reset = true;
                                return;
                            }
                            ImGui::EndPopup();
                        }

                        if (!new_log_filter_window_open) {
                            return;
                        }

                        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                        glClear(GL_COLOR_BUFFER_BIT);
                        ImGui::Render();
                        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                        SDL_GL_SwapWindow(window);
                    }
                }

                if (ImGui::BeginMenu("Restart With Different Region")) {
                    if (Settings::values.region_value != Settings::REGION_VALUE_AUTO_SELECT &&
                        ImGui::MenuItem("Auto-select")) {
                        Settings::values.region_value = Settings::REGION_VALUE_AUTO_SELECT;
                        request_reset = true;
                    }

                    if (Settings::values.region_value != 0 && ImGui::MenuItem("Japan")) {
                        Settings::values.region_value = 0;
                        request_reset = true;
                    }

                    if (Settings::values.region_value != 1 && ImGui::MenuItem("USA")) {
                        Settings::values.region_value = 1;
                        request_reset = true;
                    }

                    if (Settings::values.region_value != 2 && ImGui::MenuItem("Europe")) {
                        Settings::values.region_value = 2;
                        request_reset = true;
                    }

                    if (Settings::values.region_value != 3 && ImGui::MenuItem("Australia")) {
                        Settings::values.region_value = 3;
                        request_reset = true;
                    }

                    if (Settings::values.region_value != 4 && ImGui::MenuItem("China")) {
                        Settings::values.region_value = 4;
                        request_reset = true;
                    }

                    if (Settings::values.region_value != 5 && ImGui::MenuItem("Korea")) {
                        Settings::values.region_value = 5;
                        request_reset = true;
                    }

                    if (Settings::values.region_value != 6 && ImGui::MenuItem("Taiwan")) {
                        Settings::values.region_value = 6;
                        request_reset = true;
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Tools")) {
                if (ImGui::MenuItem("Dump RomFS")) {
                    const std::string folder = pfd::select_folder("Dump RomFS").result();

                    if (!folder.empty()) {
                        Loader::AppLoader& loader = system.GetAppLoader();

                        if (loader.DumpRomFS(folder) == Loader::ResultStatus::Success) {
                            loader.DumpUpdateRomFS(folder);
                            pfd::message("vvctre", "RomFS dumped", pfd::choice::ok);
                        } else {
                            pfd::message("vvctre", "Failed to dump RomFS", pfd::choice::ok,
                                         pfd::icon::error);
                        }
                    }
                }

                if (ImGui::BeginMenu("Files")) {
                    if (ImGui::MenuItem("Copy Cheats File Path")) {
                        const u64 program_id =
                            system.Kernel().GetCurrentProcess()->codeset->program_id;
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                fmt::format("{}{:016X}.txt",
                                            FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir),
                                            program_id),
                                FileUtil::DirectorySeparator::PlatformDefault)
                                .c_str());
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Folders")) {
                    if (ImGui::MenuItem("Copy Save Data Folder Path")) {
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                FileSys::ArchiveSource_SDSaveData::GetSaveDataPathFor(
                                    FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir),
                                    system.Kernel().GetCurrentProcess()->codeset->program_id),
                                FileUtil::DirectorySeparator::PlatformDefault)
                                .c_str());
                    }

                    if (ImGui::MenuItem("Copy Extra Data Folder Path")) {
                        u64 extdata_id = 0;
                        system.GetAppLoader().ReadExtdataId(extdata_id);
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                FileSys::GetExtDataPathFromId(
                                    FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), extdata_id),
                                FileUtil::DirectorySeparator::PlatformDefault)
                                .c_str());
                    }

                    if (ImGui::MenuItem("Copy Title Folder Path")) {
                        const u64 program_id =
                            system.Kernel().GetCurrentProcess()->codeset->program_id;
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                Service::AM::GetTitlePath(
                                    Service::AM::GetTitleMediaType(program_id), program_id))
                                .c_str());
                    }

                    if (ImGui::MenuItem("Copy Update Folder Path")) {
                        const u64 program_id =
                            system.Kernel().GetCurrentProcess()->codeset->program_id;
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                Service::AM::GetTitlePath(Service::FS::MediaType::SDMC,
                                                          0x0004000e00000000 |
                                                              static_cast<u32>(program_id)))
                                .c_str());
                    }

                    if (ImGui::MenuItem("Copy Mod Folder Path")) {
                        const u64 program_id =
                            system.Kernel().GetCurrentProcess()->codeset->program_id;
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                fmt::format("{}luma/titles/{:016X}",
                                            FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir),
                                            FileSys::GetModId(program_id)))
                                .c_str());
                    }

                    if (ImGui::MenuItem("Copy Cheats Folder Path")) {
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir))
                                .c_str());
                    }

                    if (ImGui::MenuItem("Copy SysData Folder Path")) {
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                FileUtil::GetUserPath(FileUtil::UserPath::SysDataDir))
                                .c_str());
                    }

                    if (ImGui::MenuItem("Copy Custom Textures Folder Path")) {
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                fmt::format(
                                    "{}textures/{:016X}",
                                    FileUtil::GetUserPath(FileUtil::UserPath::LoadDir),
                                    system.Kernel().GetCurrentProcess()->codeset->program_id))
                                .c_str());
                    }

                    if (ImGui::MenuItem("Copy Dumped Textures Folder Path")) {
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                fmt::format(
                                    "{}textures/{:016X}",
                                    FileUtil::GetUserPath(FileUtil::UserPath::DumpDir),
                                    system.Kernel().GetCurrentProcess()->codeset->program_id))
                                .c_str());
                    }

                    if (ImGui::MenuItem("Copy Post Processing Shaders Folder Path")) {
                        ImGui::SetClipboardText(
                            FileUtil::SanitizePath(
                                FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir))
                                .c_str());
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Screenshot")) {
                    if (ImGui::MenuItem("Save Screenshot")) {
                        const auto& layout = GetFramebufferLayout();
                        u8* data = new u8[layout.width * layout.height * 4];
                        if (VideoCore::RequestScreenshot(
                                data,
                                [=] {
                                    const auto filename =
                                        pfd::save_file("Save Screenshot", "screenshot.png",
                                                       {"Portable Network Graphics", "*.png"})
                                            .result();
                                    if (!filename.empty()) {
                                        std::vector<u8> v(layout.width * layout.height * 4);
                                        std::memcpy(v.data(), data, v.size());
                                        delete[] data;

                                        const auto convert_bgra_to_rgba =
                                            [](const std::vector<u8>& input,
                                               const Layout::FramebufferLayout& layout) {
                                                int offset = 0;
                                                std::vector<u8> output(input.size());

                                                for (u32 y = 0; y < layout.height; ++y) {
                                                    for (u32 x = 0; x < layout.width; ++x) {
                                                        output[offset] = input[offset + 2];
                                                        output[offset + 1] = input[offset + 1];
                                                        output[offset + 2] = input[offset];
                                                        output[offset + 3] = input[offset + 3];

                                                        offset += 4;
                                                    }
                                                }

                                                return output;
                                            };

                                        v = convert_bgra_to_rgba(v, layout);
                                        Common::FlipRGBA8Texture(v, static_cast<u64>(layout.width),
                                                                 static_cast<u64>(layout.height));

                                        stbi_write_png(filename.c_str(), layout.width,
                                                       layout.height, 4, v.data(),
                                                       layout.width * 4);
                                    }
                                },
                                layout)) {
                            delete[] data;
                        }
                    }

                    if (ImGui::MenuItem("Copy Screenshot")) {
                        CopyScreenshot();
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Movie")) {
                    auto& movie = Core::Movie::GetInstance();

                    if (ImGui::MenuItem("Play", nullptr, nullptr,
                                        !movie.IsPlayingInput() && !movie.IsRecordingInput())) {
                        const auto filename = pfd::open_file("Play Movie", *asl::Process::myDir(),
                                                             {"VvCtre Movie", "*.vcm"})
                                                  .result();
                        if (!filename.empty()) {
                            const Core::Movie::ValidationResult movie_result =
                                movie.ValidateMovie(filename[0]);
                            switch (movie_result) {
                            case Core::Movie::ValidationResult::OK:
                                if (asl::File(filename[0].c_str()).name().contains("loop")) {
                                    play_movie_loop_callback = [this, &movie,
                                                                filename = filename[0]] {
                                        movie.StartPlayback(filename, play_movie_loop_callback);
                                    };

                                    play_movie_loop_callback();
                                } else {
                                    movie.StartPlayback(filename[0], [&] {
                                        pfd::message("vvctre", "Playback finished",
                                                     pfd::choice::ok);
                                    });
                                }
                                break;
                            case Core::Movie::ValidationResult::GameDismatch:
                                pfd::message(
                                    "vvctre",
                                    "Movie was recorded using a ROM with a different program ID",
                                    pfd::choice::ok, pfd::icon::warning);
                                if (asl::File(filename[0].c_str()).name().contains("loop")) {
                                    play_movie_loop_callback = [this, &movie,
                                                                filename = filename[0]] {
                                        movie.StartPlayback(filename, play_movie_loop_callback);
                                    };

                                    play_movie_loop_callback();
                                } else {
                                    movie.StartPlayback(filename[0], [&] {
                                        pfd::message("vvctre", "Playback finished",
                                                     pfd::choice::ok);
                                    });
                                }
                                break;
                            case Core::Movie::ValidationResult::Invalid:
                                pfd::message("vvctre", "Movie file doesn't have a valid header",
                                             pfd::choice::ok, pfd::icon::info);
                                break;
                            }
                        }
                    }

                    if (ImGui::MenuItem("Record", nullptr, nullptr,
                                        !movie.IsPlayingInput() && !movie.IsRecordingInput())) {
                        const std::string filename =
                            pfd::save_file("Record Movie", "movie.vcm", {"VvCtre Movie", "*.vcm"})
                                .result();
                        if (!filename.empty()) {
                            movie.StartRecording(filename);
                        }
                    }

                    if (ImGui::MenuItem("Stop Playback/Recording", nullptr, nullptr,
                                        movie.IsPlayingInput() || movie.IsRecordingInput())) {
                        movie.Shutdown();
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Multiplayer")) {
                if (ImGui::MenuItem("Connect To Citra Room")) {
                    if (!ImGui::IsKeyDown(SDL_SCANCODE_LSHIFT)) {
                        all_public_rooms = GetPublicCitraRooms();
                    }
                    show_connect_to_citra_room = true;
                }
                ImGui::EndMenu();
            }

            plugin_manager.AddMenus();

            ImGui::EndPopup();
        } else if (menu_open) {
            if (play_coins_changed) {
                Service::PTM::Module::SetPlayCoins(play_coins);
                request_reset = true;
                play_coins_changed = false;
            }
            if (config_savegame_changed) {
                Service::CFG::GetModule(system)->UpdateConfigNANDSavegame();
                request_reset = true;
                config_savegame_changed = false;
            }
            if (request_reset) {
                system.RequestReset();
                request_reset = false;
            }
            amiibo_generate_and_load_search_results.clear();
            amiibo_generate_and_load_search_text.clear();
            amiibo_generate_and_load_search_text_.clear();
            amiibo_generate_and_load_custom_id.clear();
            paused = false;
            menu_open = false;
        }
    }
    ImGui::End();

    if (keyboard_data != nullptr) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin("Keyboard", nullptr,
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
            if (!keyboard_data->config.hint_text.empty()) {
                ImGui::TextUnformatted(keyboard_data->config.hint_text.c_str());
            }

            if (keyboard_data->config.multiline_mode) {
                ImGui::InputTextMultiline("##text_multiline", &keyboard_data->text);
            } else {
                ImGui::InputText("##text_one_line", &keyboard_data->text);
            }

            switch (keyboard_data->config.button_config) {
            case Frontend::ButtonConfig::None:
            case Frontend::ButtonConfig::Single: {
                if (ImGui::Button((keyboard_data->config.button_text[2].empty()
                                       ? Frontend::SWKBD_BUTTON_OKAY
                                       : keyboard_data->config.button_text[2])
                                      .c_str())) {
                    keyboard_data = nullptr;
                }
                break;
            }

            case Frontend::ButtonConfig::Dual: {
                const std::string cancel = keyboard_data->config.button_text[0].empty()
                                               ? Frontend::SWKBD_BUTTON_CANCEL
                                               : keyboard_data->config.button_text[0];
                const std::string ok = keyboard_data->config.button_text[2].empty()
                                           ? Frontend::SWKBD_BUTTON_OKAY
                                           : keyboard_data->config.button_text[2];
                if (ImGui::Button(cancel.c_str())) {
                    keyboard_data = nullptr;
                    break;
                }
                if (Frontend::SoftwareKeyboard::ValidateInput(keyboard_data->text,
                                                              keyboard_data->config) ==
                    Frontend::ValidationError::None) {
                    ImGui::SameLine();
                    if (ImGui::Button(ok.c_str())) {
                        keyboard_data->code = 1;
                        keyboard_data = nullptr;
                    }
                }
                break;
            }

            case Frontend::ButtonConfig::Triple: {
                const std::string cancel = keyboard_data->config.button_text[0].empty()
                                               ? Frontend::SWKBD_BUTTON_CANCEL
                                               : keyboard_data->config.button_text[0];
                const std::string forgot = keyboard_data->config.button_text[1].empty()
                                               ? Frontend::SWKBD_BUTTON_FORGOT
                                               : keyboard_data->config.button_text[1];
                const std::string ok = keyboard_data->config.button_text[2].empty()
                                           ? Frontend::SWKBD_BUTTON_OKAY
                                           : keyboard_data->config.button_text[2];
                if (ImGui::Button(cancel.c_str())) {
                    keyboard_data = nullptr;
                    break;
                }
                ImGui::SameLine();
                if (ImGui::Button(forgot.c_str())) {
                    keyboard_data->code = 1;
                    keyboard_data = nullptr;
                    break;
                }
                if (Frontend::SoftwareKeyboard::ValidateInput(keyboard_data->text,
                                                              keyboard_data->config) ==
                    Frontend::ValidationError::None) {
                    ImGui::SameLine();
                    if (ImGui::Button(ok.c_str())) {
                        keyboard_data->code = 2;
                        keyboard_data = nullptr;
                    }
                }
                break;
            }
            }
        }
        ImGui::End();
    }

    if (mii_selector_data != nullptr) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin((mii_selector_data->config.title.empty() ? "Mii Selector"
                                                                  : mii_selector_data->config.title)
                             .c_str(),
                         nullptr,
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
            if (ImGui::ListBoxHeader("##miis")) {
                ImGui::TextUnformatted("Standard Mii");
                ImGui::Separator();

                if (ImGui::Selectable("vvctre")) {
                    mii_selector_data->code = 0;
                    mii_selector_data->selected_mii =
                        HLE::Applets::MiiSelector::GetStandardMiiResult().selected_mii_data;
                    mii_selector_data = nullptr;
                }

                if (mii_selector_data != nullptr && !mii_selector_data->miis.empty()) {
                    ImGui::NewLine();

                    ImGui::TextUnformatted("Your Miis");
                    ImGui::Separator();

                    for (std::size_t index = 0; index < mii_selector_data->miis.size(); ++index) {
                        const HLE::Applets::MiiData& mii = mii_selector_data->miis[index];
                        if (ImGui::Selectable((Common::UTF16BufferToUTF8(mii.mii_name) +
                                               fmt::format("##{}", static_cast<u32>(mii.mii_id)))
                                                  .c_str())) {
                            mii_selector_data->code = 0;
                            mii_selector_data->selected_mii = mii;
                            mii_selector_data = nullptr;
                            break;
                        }
                    }
                }
                ImGui::ListBoxFooter();
            }

            if (mii_selector_data != nullptr && mii_selector_data->config.enable_cancel_button &&
                ImGui::Button("Cancel")) {
                mii_selector_data = nullptr;
            }
        }
        ImGui::End();
    }

    if (show_ipc_recorder_window) {
        ImGui::SetNextWindowSize(ImVec2(480, 640), ImGuiCond_Appearing);
        if (ImGui::Begin("IPC Recorder", &show_ipc_recorder_window,
                         ImGuiWindowFlags_NoSavedSettings)) {
            IPCDebugger::Recorder& r = system.Kernel().GetIPCRecorder();
            bool enabled = r.IsEnabled();

            if (ImGui::Checkbox("Enabled", &enabled)) {
                r.SetEnabled(enabled);

                if (enabled) {
                    ipc_recorder_callback =
                        r.BindCallback([&](const IPCDebugger::RequestRecord& record) {
                            const int index = record.id - ipc_recorder_id_offset;
                            if (all_ipc_records.size() > index) {
                                all_ipc_records[index] = record;
                            } else {
                                all_ipc_records.emplace_back(record);
                            }
                            if (!ipc_recorder_search_text.empty()) {
                                std::string service_name;
                                std::string function_name = "Unknown";
                                if (record.client_port.id != -1) {
                                    service_name = system.ServiceManager().GetServiceNameByPortId(
                                        static_cast<u32>(record.client_port.id));
                                }
                                if (service_name.empty()) {
                                    service_name = record.server_session.name;
                                    service_name = Common::ReplaceAll(service_name, "_Server", "");
                                    service_name = Common::ReplaceAll(service_name, "_Client", "");
                                }
                                const std::string label = fmt::format(
                                    "#{} - {} - {} (0x{:08X}) - {} - {}", record.id, service_name,
                                    record.function_name.empty() ? "Unknown" : record.function_name,
                                    record.untranslated_request_cmdbuf.empty()
                                        ? 0xFFFFFFFF
                                        : record.untranslated_request_cmdbuf[0],
                                    record.is_hle ? "HLE" : "LLE",
                                    IPC_Recorder_GetStatusString(record.status));

                                if (label.find(ipc_recorder_search_text) != std::string::npos) {
                                    ipc_recorder_search_results.push_back(record);
                                }
                            }
                        });
                } else {
                    r.UnbindCallback(ipc_recorder_callback);
                    ipc_recorder_callback = nullptr;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                ipc_recorder_id_offset += all_ipc_records.size();
                all_ipc_records.clear();
                ipc_recorder_search_results.clear();
            }
            ImGui::SameLine();
            if (ImGui::InputTextWithHint("##search", "Search", &ipc_recorder_search_text_,
                                         ImGuiInputTextFlags_EnterReturnsTrue)) {
                ipc_recorder_search_text = ipc_recorder_search_text_;
                ipc_recorder_search_results.clear();

                if (!ipc_recorder_search_text.empty()) {
                    for (const IPCDebugger::RequestRecord& record : all_ipc_records) {
                        std::string service_name;
                        std::string function_name = "Unknown";
                        if (record.client_port.id != -1) {
                            service_name = system.ServiceManager().GetServiceNameByPortId(
                                static_cast<u32>(record.client_port.id));
                        }
                        if (service_name.empty()) {
                            service_name = record.server_session.name;
                            service_name = Common::ReplaceAll(service_name, "_Server", "");
                            service_name = Common::ReplaceAll(service_name, "_Client", "");
                        }
                        const std::string label = fmt::format(
                            "#{} - {} - {} (0x{:08X}) - {} - {}", record.id, service_name,
                            record.function_name.empty() ? "Unknown" : record.function_name,
                            record.untranslated_request_cmdbuf.empty()
                                ? 0xFFFFFFFF
                                : record.untranslated_request_cmdbuf[0],
                            record.is_hle ? "HLE" : "LLE",
                            IPC_Recorder_GetStatusString(record.status));

                        if (label.find(ipc_recorder_search_text) != std::string::npos) {
                            ipc_recorder_search_results.push_back(record);
                        }
                    }
                }
            }
            const float width = ImGui::GetWindowWidth();
            if (ImGui::BeginChildFrame(ImGui::GetID("Records"), ImVec2(-1.0f, -1.0f),
                                       ImGuiWindowFlags_HorizontalScrollbar)) {
                const std::vector<IPCDebugger::RequestRecord>& records =
                    ipc_recorder_search_text.empty() ? all_ipc_records
                                                     : ipc_recorder_search_results;
                ImGuiListClipper clipper(records.size());

                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                        const IPCDebugger::RequestRecord& record = records[i];

                        std::string service_name;
                        std::string function_name = "Unknown";
                        if (record.client_port.id != -1) {
                            service_name = system.ServiceManager().GetServiceNameByPortId(
                                static_cast<u32>(record.client_port.id));
                        }
                        if (service_name.empty()) {
                            service_name = record.server_session.name;
                            service_name = Common::ReplaceAll(service_name, "_Server", "");
                            service_name = Common::ReplaceAll(service_name, "_Client", "");
                        }
                        const std::string label = fmt::format(
                            "#{} - {} - {} (0x{:08X}) - {} - {}", record.id, service_name,
                            record.function_name.empty() ? "Unknown" : record.function_name,
                            record.untranslated_request_cmdbuf.empty()
                                ? 0xFFFFFFFF
                                : record.untranslated_request_cmdbuf[0],
                            record.is_hle ? "HLE" : "LLE",
                            IPC_Recorder_GetStatusString(record.status));
                        ImGui::Selectable(label.c_str());
                        if (ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();

                            ImGui::PushTextWrapPos(width * 0.7f);

                            ImGui::TextUnformatted(
                                fmt::format(
                                    "ID: {}\n"
                                    "Status: {} ({})\n"
                                    "HLE: {}\n"
                                    "Function: {}\n"
                                    "Client Process: {} ({})\n"
                                    "Client Thread: {} ({})\n"
                                    "Client Session: {} ({})\n"
                                    "Client Port: {} ({})\n"
                                    "Server Process: {} ({})\n"
                                    "Server Thread: {} ({})\n"
                                    "Server Session: {} ({})\n"
                                    "Untranslated Request Command Buffer: 0x{:08X}\n"
                                    "Translated Request Command Buffer: 0x{:08X}\n"
                                    "Untranslated Reply Command Buffer: 0x{:08X}\n"
                                    "Translated Reply Command Buffer: 0x{:08X}",
                                    record.id, IPC_Recorder_GetStatusString(record.status),
                                    static_cast<int>(record.status), record.is_hle ? "Yes" : "No",
                                    record.function_name.empty() ? "Unknown" : record.function_name,
                                    record.client_process.name, record.client_process.id,
                                    record.client_thread.name, record.client_thread.id,
                                    record.client_session.name, record.client_session.id,
                                    record.client_port.name, record.client_port.id,
                                    record.server_process.name, record.server_process.id,
                                    record.server_thread.name, record.server_thread.id,
                                    record.server_session.name, record.server_session.id,
                                    fmt::join(record.untranslated_request_cmdbuf, ", 0x"),
                                    fmt::join(record.translated_request_cmdbuf, ", 0x"),
                                    fmt::join(record.untranslated_reply_cmdbuf, ", 0x"),
                                    fmt::join(record.translated_reply_cmdbuf, ", 0x"))
                                    .c_str());

                            ImGui::PopTextWrapPos();

                            ImGui::EndTooltip();
                        }
                    }
                }
            }
            ImGui::EndChildFrame();
        }
        if (!show_ipc_recorder_window) {
            IPCDebugger::Recorder& r = system.Kernel().GetIPCRecorder();

            r.SetEnabled(false);
            r.UnbindCallback(ipc_recorder_callback);

            all_ipc_records.clear();
            ipc_recorder_search_results.clear();
            ipc_recorder_search_text.clear();
            ipc_recorder_callback = nullptr;
        }
        ImGui::End();
    }

    if (show_cheats_window) {
        ImGui::SetNextWindowSize(ImVec2(480.0f, 640.0f), ImGuiCond_Appearing);

        if (ImGui::Begin("Cheats", &show_cheats_window, ImGuiWindowFlags_NoSavedSettings)) {
            if (ImGui::Button("Edit")) {
                std::ostringstream oss;
                system.CheatEngine().SaveCheatsToStream(oss);
                cheats_text_editor_text = oss.str();
                show_cheats_text_editor = true;
            }

            ImGui::SameLine();

            if (ImGui::Button("Reload File")) {
                system.CheatEngine().LoadCheatsFromFile();

                if (show_cheats_text_editor) {
                    std::ostringstream oss;
                    system.CheatEngine().SaveCheatsToStream(oss);
                    cheats_text_editor_text = oss.str();
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Save File")) {
                system.CheatEngine().SaveCheatsToFile();

                if (show_cheats_text_editor) {
                    const std::string filepath = fmt::format(
                        "{}{:016X}.txt", FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir),
                        system.Kernel().GetCurrentProcess()->codeset->program_id);

                    FileUtil::ReadFileToString(true, filepath, cheats_text_editor_text);
                }
            }

            if (ImGui::BeginChildFrame(ImGui::GetID("Cheats"), ImVec2(-1.0f, -1.0f),
                                       ImGuiWindowFlags_HorizontalScrollbar)) {
                const std::vector<std::shared_ptr<Cheats::CheatBase>>& cheats =
                    system.CheatEngine().GetCheats();

                ImGuiListClipper clipper(cheats.size());

                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        const std::shared_ptr<Cheats::CheatBase>& cheat = cheats[i];
                        bool enabled = cheat->IsEnabled();
                        if (ImGui::Checkbox(cheat->GetName().c_str(), &enabled)) {
                            cheat->SetEnabled(enabled);
                        }
                    }
                }
            }
            ImGui::EndChildFrame();
        }

        if (!show_cheats_window) {
            show_cheats_text_editor = false;
            cheats_text_editor_text.clear();
        }

        ImGui::End();

        if (show_cheats_text_editor) {
            ImGui::SetNextWindowSize(ImVec2(640.0f, 480.0f), ImGuiCond_Appearing);

            if (ImGui::Begin("Cheats Text Editor", &show_cheats_text_editor,
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar)) {
                if (ImGui::BeginMenuBar()) {
                    if (ImGui::BeginMenu("File")) {
                        if (ImGui::MenuItem("Save")) {
                            const std::string filepath = fmt::format(
                                "{}{:016X}.txt",
                                FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir),
                                system.Kernel().GetCurrentProcess()->codeset->program_id);

                            FileUtil::WriteStringToFile(true, filepath, cheats_text_editor_text);

                            system.CheatEngine().LoadCheatsFromFile();
                        }

                        if (ImGui::MenuItem("Load Cheats From Text")) {
                            std::istringstream iss(cheats_text_editor_text);
                            system.CheatEngine().LoadCheatsFromStream(iss);
                        }

                        ImGui::EndMenu();
                    }

                    ImGui::EndMenuBar();
                }

                ImGui::InputTextMultiline("##cheats_text_editor_text", &cheats_text_editor_text,
                                          ImVec2(-1.0f, -1.0f));
            }

            if (!show_cheats_text_editor) {
                cheats_text_editor_text.clear();
            }
        }
    }

    Network::RoomMember& room_member = system.RoomMember();
    if (room_member.GetState() == Network::RoomMember::State::Joined) {
        ImGui::SetNextWindowSize(ImVec2(640.f, 480.0f), ImGuiCond_Appearing);

        bool open = true;
        const Network::RoomInformation& room_information = room_member.GetRoomInformation();
        const Network::RoomMember::MemberList& members = room_member.GetMemberInformation();

        if (ImGui::Begin(fmt::format("{} ({}/{})###Room", room_information.name, members.size(),
                                     room_information.member_slots)
                             .c_str(),
                         &open, ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::PushTextWrapPos();
            ImGui::TextUnformatted(room_information.description.c_str());
            ImGui::PopTextWrapPos();

            ImGui::Columns(2);

            if (ImGui::BeginChildFrame(ImGui::GetID("Members"), ImVec2(-1.0f, -1.0f),
                                       ImGuiWindowFlags_HorizontalScrollbar)) {
                ImGuiListClipper clipper(members.size());

                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        const Network::RoomMember::MemberInformation& member = members[i];

                        if (member.game_info.name.empty()) {
                            ImGui::TextUnformatted(member.nickname.c_str());
                        } else {
                            ImGui::Text("%s is playing %s", member.nickname.c_str(),
                                        member.game_info.name.c_str());
                        }
                        if (member.nickname != room_member.GetNickname()) {
                            if (ImGui::BeginPopupContextItem(member.nickname.c_str(),
                                                             ImGuiMouseButton_Right)) {
                                if (multiplayer_blocked_nicknames.count(member.nickname)) {
                                    if (ImGui::MenuItem("Unblock")) {
                                        multiplayer_blocked_nicknames.erase(member.nickname);
                                    }
                                } else {
                                    if (ImGui::MenuItem("Block")) {
                                        multiplayer_blocked_nicknames.insert(member.nickname);
                                    }
                                }

                                ImGui::EndPopup();
                            }
                        }
                    }
                }
            }
            ImGui::EndChildFrame();

            ImGui::NextColumn();

            const ImVec2 spacing = style.ItemSpacing;

            if (ImGui::BeginChildFrame(ImGui::GetID("Messages"),
                                       ImVec2(-1.0f, -ImGui::GetFrameHeightWithSpacing()),
                                       ImGuiWindowFlags_HorizontalScrollbar)) {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

                ImGuiListClipper clipper(multiplayer_messages.size());

                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        ImGui::TextUnformatted(multiplayer_messages[i].c_str());

                        if (ImGui::BeginPopupContextItem("Message Menu")) {
                            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, spacing);

                            if (ImGui::MenuItem("Delete")) {
                                multiplayer_messages.erase(multiplayer_messages.begin() + i);
                                clipper.ItemsCount = 0;
                                ImGui::PopStyleVar();
                                ImGui::EndPopup();
                                break;
                            }

                            if (ImGui::MenuItem("Clear")) {
                                multiplayer_messages.clear();
                                clipper.ItemsCount = 0;
                                ImGui::PopStyleVar();
                                ImGui::EndPopup();
                                break;
                            }

                            if (ImGui::MenuItem("Copy")) {
                                ImGui::SetClipboardText(multiplayer_messages[i].c_str());
                            }

                            if (ImGui::MenuItem("Copy All")) {
                                std::string all;
                                for (std::size_t j = 0; j < multiplayer_messages.size(); ++j) {
                                    if (j > 0) {
                                        all += '\n';
                                    }
                                    all += multiplayer_messages[j];
                                }
                                ImGui::SetClipboardText(all.c_str());
                            }

                            ImGui::PopStyleVar();

                            ImGui::EndPopup();
                        }
                    }
                }

                ImGui::PopStyleVar();

                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChildFrame();

            ImGui::PushItemWidth(-1.0f);
            if (ImGui::InputTextWithHint("##message", "Send Chat Message", &multiplayer_message,
                                         ImGuiInputTextFlags_EnterReturnsTrue)) {
                room_member.SendChatMessage(multiplayer_message);
                multiplayer_message.clear();
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::PopItemWidth();

            ImGui::Columns();

            ImGui::EndPopup();
        }
        if (!open) {
            room_member.Leave();
        }
    }

    if (!all_installed.empty()) {
        ImGui::OpenPopup("Installed");

        ImGui::SetNextWindowPos(ImVec2());
        ImGui::SetNextWindowSize(io.DisplaySize);

        bool open = true;

        if (ImGui::BeginPopupModal("Installed", &open,
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoResize)) {
            if (ImGui::InputTextWithHint("##search", "Search", &installed_search_text_,
                                         ImGuiInputTextFlags_EnterReturnsTrue)) {
                installed_search_text = installed_search_text_;
                installed_search_results.clear();

                if (!installed_search_text.empty()) {
                    for (const auto& title : all_installed) {
                        const auto [path, name] = title;

                        if (asl::String(name.c_str())
                                .toLowerCase()
                                .contains(
                                    asl::String(installed_search_text.c_str()).toLowerCase())) {
                            installed_search_results.push_back(title);
                        }
                    }
                }
            }

            if (ImGui::BeginChildFrame(ImGui::GetID("Installed"), ImVec2(-1.0f, -1.0f),
                                       ImGuiWindowFlags_HorizontalScrollbar)) {
                const auto& v =
                    installed_search_text.empty() ? all_installed : installed_search_results;
                ImGuiListClipper clipper(v.size());

                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        const auto [path, name] = v[i];

                        if (ImGui::Selectable(name.c_str())) {
                            system.SetResetFilePath(path);
                            system.RequestReset();
                            all_installed.clear();
                            installed_search_results.clear();
                            installed_search_text.clear();
                            installed_search_text_.clear();
                            break;
                        }
                    }
                }
            }
            ImGui::EndChildFrame();
            ImGui::EndPopup();
        }
        if (!open) {
            all_installed.clear();
            installed_search_results.clear();
            installed_search_text.clear();
            installed_search_text_.clear();
        }
    }

    if (show_connect_to_citra_room) {
        ImGui::OpenPopup("Connect To Citra Room");

        ImGui::SetNextWindowPos(ImVec2());
        ImGui::SetNextWindowSize(io.DisplaySize);

        if (ImGui::BeginPopupModal("Connect To Citra Room", &show_connect_to_citra_room,
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoResize)) {
            ImGui::InputText("IP", &Settings::values.multiplayer_ip);
            ImGui::InputScalar("Port", ImGuiDataType_U16, &Settings::values.multiplayer_port);
            ImGui::InputText("Nickname", &Settings::values.multiplayer_nickname);
            ImGui::InputText("Password", &Settings::values.multiplayer_password);

            ImGui::NewLine();
            ImGui::TextUnformatted("Public Rooms");

            if (ImGui::Button("Refresh")) {
                all_public_rooms = GetPublicCitraRooms();

                public_rooms_search_text = public_rooms_search_text_;
                public_rooms_search_results.clear();

                asl::String lower_case_text =
                    asl::String(public_rooms_search_text.c_str()).toLowerCase();

                if (!public_rooms_search_text.empty()) {
                    for (const CitraRoom& room : all_public_rooms) {
                        if (asl::String(room.name.c_str())
                                .toLowerCase()
                                .contains(lower_case_text) ||
                            asl::String(GetRoomPopupText(room).c_str())
                                .toLowerCase()
                                .contains(lower_case_text)) {
                            public_rooms_search_results.push_back(room);
                        }
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::InputTextWithHint("##search", "Search", &public_rooms_search_text_,
                                         ImGuiInputTextFlags_EnterReturnsTrue)) {
                public_rooms_search_text = public_rooms_search_text_;
                public_rooms_search_results.clear();

                asl::String lower_case_text =
                    asl::String(public_rooms_search_text.c_str()).toLowerCase();

                if (!public_rooms_search_text.empty()) {
                    for (const CitraRoom& room : all_public_rooms) {
                        if (asl::String(room.name.c_str())
                                .toLowerCase()
                                .contains(lower_case_text) ||
                            asl::String(GetRoomPopupText(room).c_str())
                                .toLowerCase()
                                .contains(lower_case_text)) {
                            public_rooms_search_results.push_back(room);
                        }
                    }
                }
            }

            if (ImGui::BeginChildFrame(ImGui::GetID("Public Room List"),
                                       ImVec2(-1.0f, -ImGui::GetFrameHeightWithSpacing() * 2.0f),
                                       ImGuiWindowFlags_HorizontalScrollbar)) {
                const CitraRoomList& rooms = public_rooms_search_text.empty()
                                                 ? all_public_rooms
                                                 : public_rooms_search_results;

                ImGuiListClipper clipper(rooms.size());

                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        const CitraRoom& room = rooms[i];
                        const std::string popup_text = GetRoomPopupText(room);
                        const std::string id =
                            fmt::format("{}##ip={},port={},popup_text={}", room.name, room.ip,
                                        room.port, popup_text);
                        if (room.has_password) {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
                            ImGui::Selectable(id.c_str());
                            ImGui::PopStyleColor();
                        } else {
                            ImGui::Selectable(id.c_str());
                        }
                        if (ImGui::IsItemClicked()) {
                            ImGui::OpenPopup(id.c_str());
                        }
                        if (ImGui::BeginPopup(id.c_str(), ImGuiWindowFlags_HorizontalScrollbar)) {
                            ImGui::TextUnformatted(popup_text.c_str());
                            if (ImGui::Button("Set IP And Port")) {
                                Settings::values.multiplayer_ip = room.ip;
                                Settings::values.multiplayer_port = room.port;
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Close")) {
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::EndPopup();
                        }
                    }
                }
            }
            ImGui::EndChildFrame();

            ImGui::NewLine();

            if (ImGui::Button("Connect")) {
                ConnectToCitraRoom();
                show_connect_to_citra_room = false;
                all_public_rooms.clear();
                public_rooms_search_results.clear();
                public_rooms_search_text.clear();
                public_rooms_search_text_.clear();
            }

            ImGui::EndPopup();
        }
        if (!show_connect_to_citra_room) {
            all_public_rooms.clear();
            public_rooms_search_results.clear();
            public_rooms_search_text.clear();
            public_rooms_search_text_.clear();
        }
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);

    plugin_manager.AfterSwapWindow();
}

void EmuWindow_SDL2::PollEvents() {
    SDL_Event event;

    // SDL_PollEvent returns 0 when there are no more events in the event queue
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        switch (event.type) {
        case SDL_WINDOWEVENT:
            switch (event.window.event) {
            case SDL_WINDOWEVENT_SIZE_CHANGED:
            case SDL_WINDOWEVENT_RESIZED:
            case SDL_WINDOWEVENT_MAXIMIZED:
            case SDL_WINDOWEVENT_RESTORED:
            case SDL_WINDOWEVENT_MINIMIZED:
                OnResize();
                break;
            default:
                break;
            }
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                !ImGui::GetIO().WantCaptureKeyboard) {
                OnKeyEvent(static_cast<int>(event.key.keysym.scancode), event.key.state);
            }

            break;
        case SDL_MOUSEMOTION:
            if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                !ImGui::GetIO().WantCaptureMouse && event.button.which != SDL_TOUCH_MOUSEID) {
                OnMouseMotion(event.motion.x, event.motion.y);
            }

            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                !ImGui::GetIO().WantCaptureMouse && event.button.which != SDL_TOUCH_MOUSEID) {
                OnMouseButton(event.button.button, event.button.state, event.button.x,
                              event.button.y);
            }

            break;
        case SDL_FINGERDOWN:
            if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                !ImGui::GetIO().WantCaptureMouse) {
                OnFingerDown(event.tfinger.x, event.tfinger.y);
            }

            break;
        case SDL_FINGERMOTION:
            if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                !ImGui::GetIO().WantCaptureMouse) {
                OnFingerMotion(event.tfinger.x, event.tfinger.y);
            }

            break;
        case SDL_FINGERUP:
            if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                !ImGui::GetIO().WantCaptureMouse) {
                OnFingerUp();
            }

            break;
        case SDL_QUIT:
            if (pfd::message("vvctre", "Would you like to exit now?", pfd::choice::yes_no,
                             pfd::icon::question)
                    .result() == pfd::button::yes) {
                is_open = false;
            }

            break;
        default:
            break;
        }
    }
}

void EmuWindow_SDL2::CopyScreenshot() {
    const auto& layout = GetFramebufferLayout();
    u8* data = new u8[layout.width * layout.height * 4];

    if (VideoCore::RequestScreenshot(
            data,
            [=] {
                std::vector<u8> v(layout.width * layout.height * 4);
                std::memcpy(v.data(), data, v.size());
                delete[] data;

                const auto convert_bgra_to_rgba = [](const std::vector<u8>& input,
                                                     const Layout::FramebufferLayout& layout) {
                    int offset = 0;
                    std::vector<u8> output(input.size());

                    for (u32 y = 0; y < layout.height; ++y) {
                        for (u32 x = 0; x < layout.width; ++x) {
                            output[offset] = input[offset + 2];
                            output[offset + 1] = input[offset + 1];
                            output[offset + 2] = input[offset];
                            output[offset + 3] = input[offset + 3];

                            offset += 4;
                        }
                    }

                    return output;
                };

                v = convert_bgra_to_rgba(v, layout);
                Common::FlipRGBA8Texture(v, static_cast<u64>(layout.width),
                                         static_cast<u64>(layout.height));

                clip::image_spec spec;
                spec.width = layout.width;
                spec.height = layout.height;
                spec.bits_per_pixel = 32;
                spec.bytes_per_row = spec.width * 4;
                spec.red_mask = 0xff;
                spec.green_mask = 0xff00;
                spec.blue_mask = 0xff0000;
                spec.alpha_mask = 0xff000000;
                spec.red_shift = 0;
                spec.green_shift = 8;
                spec.blue_shift = 16;
                spec.alpha_shift = 24;

                clip::set_image(clip::image(v.data(), spec));
            },
            layout)) {
        delete[] data;
    }
}

void EmuWindow_SDL2::ConnectToCitraRoom() {
    if (!Settings::values.multiplayer_ip.empty() && Settings::values.multiplayer_port != 0 &&
        !Settings::values.multiplayer_nickname.empty()) {
        system.RoomMember().Join(
            Settings::values.multiplayer_nickname, Service::CFG::GetConsoleIdHash(system),
            Settings::values.multiplayer_ip.c_str(), Settings::values.multiplayer_port,
            Network::NO_PREFERRED_MAC_ADDRESS, Settings::values.multiplayer_password);
    }
}
