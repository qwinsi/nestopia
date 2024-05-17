/*
 * Nestopia UE
 *
 * Copyright (C) 2012-2024 R. Danbrook
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>

#include "uiadapter.h"

#include "inputmanager.h"

#include "jg/jg_nes.h"

namespace {

UiAdapter uiadpt;

jg_inputstate_t coreinput[5];
jg_inputinfo_t *inputinfo[5];

jg_inputstate_t uistate;
jg_inputinfo_t uiinfo;

constexpr size_t NDEFS_UI = 12;
const char *defs_ui[NDEFS_UI] = {
    "ResetSoft", "ResetHard", "FDSNextSide", "FDSInsertEject",
    "QuickSave1", "QuickSave2", "QuickLoad1", "QuickLoad2",
    "Fullscreen", "Pause", "FastForward", "Screenshot"
};

bool uiprev[NDEFS_UI];

uint8_t undef8;
uint16_t undef16;

constexpr int DEADZONE = 5120;
constexpr size_t MAXPORTS = 4;

SDL_Joystick *joystick[MAXPORTS];
int jsports[MAXPORTS];
int jsiid[MAXPORTS];

}

InputManager::InputManager(JGManager& jgm, SettingManager& setmgr)
        : jgm(jgm), setmgr(setmgr) {
    for (size_t i = 0; i < jgm.get_coreinfo()->numinputs; ++i) {
        jg_set_inputstate(&coreinput[i], i);
    }

    // Set up UI inputs
    uiinfo.type = JG_INPUT_EXTERNAL;
    uiinfo.index = 21;
    uiinfo.name = "ui";
    uiinfo.fname = "User Interface";
    uiinfo.defs = defs_ui;
    uiinfo.numaxes = 0;
    uiinfo.numbuttons = NDEFS_UI;
    uistate.button = (uint8_t*)calloc(uiinfo.numbuttons, sizeof(uint8_t));
}

InputManager::~InputManager() {
    unassign();
    free(uistate.button);
}

void InputManager::reassign() {
    unassign();
    assign();
}

void InputManager::assign() {
    for (size_t i = 0; i < jgm.get_coreinfo()->numinputs; ++i) {
        inputinfo[i] = jgm.get_inputinfo(i);

        coreinput[i].axis = (int16_t*)calloc(inputinfo[i]->numaxes, sizeof(int16_t));
        coreinput[i].button = (uint8_t*)calloc(inputinfo[i]->numbuttons, sizeof(uint8_t));

        // There are always X, Y, and Z coords
        coreinput[i].coord = (int32_t*)calloc(3, sizeof(int32_t)); // Magic Number

        // There is always X and Y relative motion
        coreinput[i].rel = (int32_t*)calloc(2, sizeof(int32_t)); // Magic Number

        if (inputinfo[i]->type == JG_INPUT_POINTER || inputinfo[i]->type == JG_INPUT_GUN) {
            msmap[0] = &coreinput[i].coord[0];
            msmap[1] = &coreinput[i].coord[1];
        }

        if (inputinfo[i]->type == JG_INPUT_GUN) {
            lightgun = true;
        }

        for (size_t j = 0; j < inputinfo[i]->numbuttons; ++j) {
            // Keyboard/Mouse
            std::string val = setmgr.get_input(inputinfo[i]->name,
                                               inputinfo[i]->defs[j + inputinfo[i]->numaxes]);
            if (!val.empty()) {
                kbmap[std::stoi(val)] = &coreinput[i].button[j];
            }
        }
    }

    // Set up UI definitiions
    int ui_defaults[NDEFS_UI] = {
        0xffbd + 1, 0xffbd + 2, 0xffbd + 3, 0xffbd + 4, 0xffbd + 5,
        0xffbd + 6, 0xffbd + 7, 0xffbd + 8, 'f', 'p', '`', 0xffbd + 9
    };

    for (size_t i = 0; i < NDEFS_UI; ++i) {
        // Keyboard/Mouse
        std::string val = setmgr.get_input("ui", uiinfo.defs[i]);
        if (val.empty()) {
            setmgr.set_input("ui", uiinfo.defs[i], std::to_string(ui_defaults[i]));
            kbmap[ui_defaults[i]] = &uistate.button[i];
        }
        else {
            kbmap[std::stoi(val)] = &uistate.button[i];
        }
    }

    remap_js();
}

void InputManager::unassign() {
    jxmap.clear();
    jamap.clear();
    jbmap.clear();
    jhmap.clear();
    kbmap.clear();
    msmap.clear();

    lightgun = false;

    for (size_t i = 0; i < jgm.get_coreinfo()->numinputs; ++i) {
        if (coreinput[i].axis) {
            free(coreinput[i].axis);
            coreinput[i].axis = nullptr;
        }
        if (coreinput[i].button) {
            free(coreinput[i].button);
            coreinput[i].button = nullptr;
        }
        if (coreinput[i].coord) {
            free(coreinput[i].coord);
            coreinput[i].coord = nullptr;
        }
        if (coreinput[i].rel) {
            free(coreinput[i].rel);
            coreinput[i].rel = nullptr;
        }
    }
}

void InputManager::remap_js() {
    jxmap.clear();
    jamap.clear();
    jbmap.clear();
    jhmap.clear();

    for (size_t i = 0; i < jgm.get_coreinfo()->numinputs; ++i) {
        inputinfo[i] = jgm.get_inputinfo(i);

        // Pattern for joystick axis definitions mapped to emulated axes
        std::regex pattern{"j[0-9][a]\\d+"};

        for (size_t j = 0; j < inputinfo[i]->numaxes; ++j) {
            std::string val = setmgr.get_input(std::string(inputinfo[i]->name) + "j",
                                               inputinfo[i]->defs[j]);
            if (val.empty()) {
                continue;
            }

            if (std::regex_match(val, pattern)) {
                int port = val[1] - '0';
                int inum = std::stoi(std::string(&val[3]));
                jxmap[(port * 100) + (inum / 2)] = &coreinput[i].axis[j];
            }
            else {
                printf("Malformed input code: %s\n", val.c_str());
            }
        }

        // Reset pattern for axes acting as buttons, buttons, and hats
        pattern = "j[0-9][abh]\\d+";

        for (size_t j = 0; j < inputinfo[i]->numbuttons; ++j) {
            // Joystick
            std::string val = setmgr.get_input(std::string(inputinfo[i]->name) + "j",
                                   inputinfo[i]->defs[j + inputinfo[i]->numaxes]);
            if (val.empty()) {
                continue;
            }

            if (std::regex_match(val, pattern)) {
                int port = val[1] - '0';
                int inum = std::stoi(std::string(&val[3]));

                if (val[2] == 'b') {
                    jbmap[(port * 100) + inum] = &coreinput[i].button[j];
                }
                else if (val[2] == 'h') {
                    jhmap[(port * 100) + inum] = &coreinput[i].button[j];
                }
                else if (val[2] == 'a') {
                    jamap[(port * 100) + inum] = &coreinput[i].button[j];
                }
            }
            else {
                printf("Malformed input code: %s\n", val.c_str());
            }
        }
    }

    for (size_t i = 0; i < NDEFS_UI; ++i) {
        std::string val = setmgr.get_input("uij", uiinfo.defs[i]);
        if (!val.empty()) {
            if (val[0] == 'j') {
                int port = val[1] - '0';
                int inum = std::stoi(std::string(&val[3]));

                if (val[2] == 'b') {
                    jbmap[(port * 100) + inum] = &uistate.button[i];
                }
                else if (val[2] == 'h') {
                    jhmap[(port * 100) + inum] = &uistate.button[i];
                }
                else if (val[2] == 'a') {
                    jamap[(port * 100) + inum] = &uistate.button[i];
                }
            }
        }
    }
}

void InputManager::set_inputdef(SDL_Event& evt) {
    // Check if an axis is being assigned
    bool axis = false;
    if ((cfg_name == "arkanoid" || cfg_name == "pachinko") && cfg_defnum < 1) {
        axis = true;
    }

    switch (evt.type) {
        case SDL_JOYBUTTONDOWN: {
            if (axis) {
                printf("Tried to configure an axis as a button\n");
                break;
            }

            SDL_Joystick *js = SDL_JoystickFromInstanceID(evt.jbutton.which);
            int port = SDL_JoystickGetPlayerIndex(js);
            int btn = evt.jbutton.button;
            set_cfg_running(false);
            setmgr.set_input(cfg_name + "j", cfg_def,
                             "j" + std::to_string(port) + "b" + std::to_string(btn));
            break;
        }
        case SDL_JOYHATMOTION: {
            if (axis) {
                printf("Tried to configure an axis as a hat\n");
                break;
            }

            SDL_Joystick *js = SDL_JoystickFromInstanceID(evt.jhat.which);
            int port = SDL_JoystickGetPlayerIndex(js);
            int hat = 0;
            if (evt.jhat.value & SDL_HAT_UP) {
                hat = 0;
            }
            else if (evt.jhat.value & SDL_HAT_DOWN) {
                hat = 1;
            }
            else if (evt.jhat.value & SDL_HAT_LEFT) {
                hat = 2;
            }
            else if (evt.jhat.value & SDL_HAT_RIGHT) {
                hat = 3;
            }
            set_cfg_running(false);
            setmgr.set_input(cfg_name + "j", cfg_def,
                             "j" + std::to_string(port) + "h" + std::to_string(hat));
            break;
        }
        case SDL_JOYAXISMOTION: {
            SDL_Joystick *js = SDL_JoystickFromInstanceID(evt.jhat.which);
            int port = SDL_JoystickGetPlayerIndex(js);
            int axis = evt.jaxis.axis * 2 + (evt.jaxis.value > 0 ? 1 : 0);
            if (abs(evt.jaxis.value) >= DEADZONE) {
                set_cfg_running(false);
                setmgr.set_input(cfg_name + "j", cfg_def,
                                 "j" + std::to_string(port) + "a" + std::to_string(axis));
            }
            break;
        }
    }

    // Early return if nothing new was defined
    if (cfg_running) {
        return;
    }

    // Remap the joysticks
    remap_js();
}

void InputManager::event(SDL_Event& evt) {
    if (cfg_running) {
        set_inputdef(evt);
        return;
    }

    switch (evt.type) {
        case SDL_JOYDEVICEADDED: {
            int port = 0;

            // Choose next unplugged port
            for (int i = 0; i < MAXPORTS; ++i) {
                if (!jsports[i]) {
                    joystick[i] = SDL_JoystickOpen(evt.jdevice.which);
                    SDL_JoystickSetPlayerIndex(joystick[i], i);
                    jsports[i] = 1;
                    jsiid[i] = SDL_JoystickInstanceID(joystick[i]);
                    port = i;
                    break;
                }
            }

            printf("Joystick %d Connected: %s (Instance ID: %d)\n",
                   SDL_JoystickGetPlayerIndex(joystick[port]) + 1,
                   SDL_JoystickName(joystick[port]),
                   jsiid[port]);

            break;
        }
        case SDL_JOYDEVICEREMOVED: {
            int id = evt.jdevice.which;
            for (int i = 0; i < MAXPORTS; ++i) {
                if (jsiid[i] == id) {
                    jsports[i] = 0;
                    printf("Joystick %d Disconnected (Instance ID: %d)\n",
                           i + 1, id);
                    SDL_JoystickClose(joystick[i]);
                    break;
                }
            }
            break;
        }

        case SDL_JOYBUTTONUP: {
            SDL_Joystick *js = SDL_JoystickFromInstanceID(evt.jbutton.which);
            int port = SDL_JoystickGetPlayerIndex(js);
            int btn = (port * 100) + evt.jbutton.button;
            if (jbmap[btn] != nullptr) {
                *jbmap[btn] = 0;
            }
            break;
        }
        case SDL_JOYBUTTONDOWN: {
            SDL_Joystick *js = SDL_JoystickFromInstanceID(evt.jbutton.which);
            int port = SDL_JoystickGetPlayerIndex(js);
            int btn = (port * 100) + evt.jbutton.button;
            if (jbmap[btn] != nullptr) {
                *jbmap[btn] = 1;
            }
            break;
        }

        case SDL_JOYHATMOTION: {
            SDL_Joystick *js = SDL_JoystickFromInstanceID(evt.jhat.which);
            int port = SDL_JoystickGetPlayerIndex(js);
            int hat = (port * 100);
            if (jhmap[hat + 0] != nullptr) {
                *jhmap[hat + 0] = evt.jhat.value & SDL_HAT_UP;
            }
            if (jhmap[hat + 1] != nullptr) {
                *jhmap[hat + 1] = (evt.jhat.value & SDL_HAT_DOWN) >> 2;
            }
            if (jhmap[hat + 2] != nullptr) {
                *jhmap[hat + 2] = (evt.jhat.value & SDL_HAT_LEFT) >> 3;
            }
            if (jhmap[hat + 3] != nullptr) {
                *jhmap[hat + 3] = (evt.jhat.value & SDL_HAT_RIGHT) >> 1;
            }
            break;
        }

        case SDL_JOYAXISMOTION: {
            SDL_Joystick *js = SDL_JoystickFromInstanceID(evt.jhat.which);
            int port = SDL_JoystickGetPlayerIndex(js);
            int axis = (port * 100) + (evt.jaxis.axis * 2);

            // Buttons
            if (jamap[axis] != nullptr) {
                *jamap[axis] = evt.jaxis.value < -16384;
            }
            if (jamap[axis + 1] != nullptr) {
                *jamap[axis + 1] = evt.jaxis.value > 16384;
            }

            // Analogue
            axis = (port * 100) + evt.jaxis.axis;
            if (jxmap[axis] != nullptr) {
                *jxmap[axis] = abs(evt.jaxis.value) > DEADZONE ? evt.jaxis.value : 0;
            }
            
            break;
        }
    }

    ui_events();
}

void InputManager::event(int key, bool pressed) {
    if (kbmap[key] != nullptr) {
        *kbmap[key] = pressed;
    }
}

void InputManager::event(int x, int y) {
    if (msmap[0] != nullptr) {
        *msmap[0] = x;
        *msmap[1] = y;
    }
}

void InputManager::ui_events() {
    // Process any UI events - need to make sure it went from true to false to
    // emulate a "keyup" event
    for (size_t i = 0; i < NDEFS_UI; ++i) {
        if (uiprev[i] && !uistate.button[i]) {
            switch (i) {
                case 0: // ResetSoft
                    jgm.reset(0);
                    break;
                case 1: // ResetHard
                    jgm.reset(1);
                    break;
                case 2: // FDSInsertEject
                    jgm.media_insert();
                    break;
                case 3: // FDSNextSide
                    jgm.media_select();
                    break;
                case 4: // QuickSave1
                    jgm.state_qsave(0);
                    break;
                case 5: // QuickSave2
                    jgm.state_qsave(1);
                    break;
                case 6: // QuickLoad1
                    jgm.state_qload(0);
                    break;
                case 7: // QuickLoad2
                    jgm.state_qload(1);
                    break;
                case 8: // Fullscreen
                    uiadpt.fullscreen();
                    break;
                case 9: // Pause
                    uiadpt.pause();
                    break;
                case 10: // FastForward
                    uiadpt.fastforward(false);
                    break;
                case 11: // Screenshot
                    uiadpt.screenshot();
                    break;
            }
        }

        uiprev[i] = uistate.button[i];
    }

    if (uistate.button[10]) {
        uiadpt.fastforward(true);
    }
}

std::vector<jg_inputinfo_t> InputManager::get_inputinfo() {
    std::vector<jg_inputinfo_t> input_info{};

    input_info.push_back(jg_nes_inputinfo(0, JG_NES_PAD1));
    input_info.push_back(jg_nes_inputinfo(1, JG_NES_PAD2));
    input_info.push_back(jg_nes_inputinfo(2, JG_NES_PAD3));
    input_info.push_back(jg_nes_inputinfo(3, JG_NES_PAD4));
    input_info.push_back(jg_nes_inputinfo(4, JG_NES_ZAPPER));
    input_info.push_back(jg_nes_inputinfo(5, JG_NES_ARKANOID));
    input_info.push_back(jg_nes_inputinfo(6, JG_NES_POWERPAD));
    input_info.push_back(jg_nes_inputinfo(7, JG_NES_POWERGLOVE));
    input_info.push_back(jg_nes_inputinfo(8, JG_NES_FAMILYTRAINER));
    input_info.push_back(jg_nes_inputinfo(9, JG_NES_PACHINKO));
    input_info.push_back(jg_nes_inputinfo(10, JG_NES_OEKAKIDSTABLET));
    input_info.push_back(jg_nes_inputinfo(11, JG_NES_KONAMIHYPERSHOT));
    input_info.push_back(jg_nes_inputinfo(12, JG_NES_BANDAIHYPERSHOT));
    input_info.push_back(jg_nes_inputinfo(13, JG_NES_CRAZYCLIMBER));
    input_info.push_back(jg_nes_inputinfo(14, JG_NES_MAHJONG));
    input_info.push_back(jg_nes_inputinfo(15, JG_NES_EXCITINGBOXING));
    input_info.push_back(jg_nes_inputinfo(16, JG_NES_TOPRIDER));
    input_info.push_back(jg_nes_inputinfo(17, JG_NES_POKKUNMOGURAA));
    input_info.push_back(jg_nes_inputinfo(18, JG_NES_PARTYTAP));
    input_info.push_back(jg_nes_inputinfo(19, JG_NES_VSSYS));
    input_info.push_back(jg_nes_inputinfo(20, JG_NES_KARAOKESTUDIO));

    input_info.push_back(uiinfo); // Special case

    return input_info;
}

std::string InputManager::get_inputdef(std::string device, std::string def) {
    std::string ret = setmgr.get_input(device, def);
    return ret;
}

void InputManager::clear_inputdef() {
    setmgr.set_input(cfg_name, cfg_def, std::string{});
    setmgr.set_input(cfg_name + "j", cfg_def, std::string{});
}

void InputManager::set_inputcfg(std::string name, std::string def, int defnum) {
    set_cfg_running(true);
    cfg_name = name;
    cfg_def = def;
    cfg_defnum = defnum;
}

void InputManager::set_inputdef(int val) {
    std::string current = setmgr.get_input(cfg_name, cfg_def);
    setmgr.set_input(cfg_name, cfg_def, std::to_string(val));

    if (!jgm.is_loaded()) {
        return;
    }

    int cur = std::atoi(current.c_str());

    if (kbmap[cur] != nullptr) { // replace it
        uint8_t *curptr = kbmap[cur];
        kbmap[cur] = nullptr;
        kbmap[val] = (uint8_t*)curptr;
    }
    else { // freshly place it in the map
        for (size_t i = 0; i < jgm.get_coreinfo()->numinputs; ++i) {
            if (std::string(inputinfo[i]->name) == cfg_name) {
                kbmap[val] = &coreinput[i].button[cfg_defnum];
            }
        }
    }
}

void InputManager::set_cfg_running(bool running) {
    cfg_running = running;
    if (!running) {
        // Turn off the message now
        uiadpt.show_msgbox(running);
    }
}
