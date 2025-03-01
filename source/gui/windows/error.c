/******************************************************************************\
**
**  This file is part of the Hades GBA Emulator, and is made available under
**  the terms of the GNU General Public License version 2.
**
**  Copyright (C) 2021-2023 - The Hades Authors
**
\******************************************************************************/

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>
#include "hades.h"
#include "app.h"
#include "gui/gui.h"

void
gui_new_error(
    struct app *app,
    char *msg
) {
    free(app->ui.error.msg);
    app->ui.error.msg = msg;
    app->ui.error.active = true;
}

void
gui_win_error(
    struct app *app
) {
    ImVec4 bg;

    if (app->ui.error.active) {
        app->ui.error.active = false;
        igOpenPopup("Error", ImGuiPopupFlags_None);
        logln(HS_ERROR, "Error: %s", app->ui.error.msg);
    }

    // #B2354E
    bg = (ImVec4) {
        .x = 178.f / 255.f,
        .y =  53.f / 255.f,
        .z =  78.f / 255.f,
        .w = 1.f,
    };

    igPushStyleColorVec4(ImGuiCol_PopupBg, bg);
    igPushStyleColorVec4(ImGuiCol_Button, (ImVec4){.x = 1.f, .y = 1.f, .z = 1.f, .w = 0.25});
    igPushStyleColorVec4(ImGuiCol_ButtonHovered, (ImVec4){.x = 1.f, .y = 1.f, .z = 1.f, .w = 0.4});
    igPushStyleColorVec4(ImGuiCol_ButtonActive, (ImVec4){.x = 1.f, .y = 1.f, .z = 1.f, .w = 0.5});
    igSetNextWindowSize((ImVec2){.x = igGetFontSize() * 25.f, .y = 0.f}, ImGuiCond_Always);

    if (igBeginPopupModal(
        "Error",
        NULL,
        ImGuiWindowFlags_Popup | ImGuiWindowFlags_Modal | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar
    )) {
        igTextWrapped("Error: %s", app->ui.error.msg);
        igSpacing();
        igSpacing();
        igSpacing();

        igPushStyleColorVec4(ImGuiCol_Text, (ImVec4){.x = 1.f, .y = 1.f, .z = 1.f, .w = 1.0});

        if (igButton("Close", (ImVec2){.x = igGetFontSize() * 4.f, .y = igGetFontSize() * 1.5f})) {
            igCloseCurrentPopup();
        }

        igPopStyleColor(1);

        igEndPopup();
    }
    igPopStyleColor(4);
}
