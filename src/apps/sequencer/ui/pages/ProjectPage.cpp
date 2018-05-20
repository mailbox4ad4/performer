#include "ProjectPage.h"

#include "ui/LedPainter.h"
#include "ui/pages/Pages.h"
#include "ui/painters/WindowPainter.h"

#include "model/FileManager.h"

#include "core/fs/FileSystem.h"
#include "core/utils/StringBuilder.h"

enum class ContextAction {
    Init,
    Load,
    Save,
    SaveAs,
    Route,
    Last
};

static const ContextMenuModel::Item contextMenuItems[] = {
    { "INIT" },
    { "LOAD" },
    { "SAVE" },
    { "SAVE AS" },
    { "ROUTE" }
};

ProjectPage::ProjectPage(PageManager &manager, PageContext &context) :
    ListPage(manager, context, _listModel),
    _listModel(context.model.project())
{}

void ProjectPage::enter() {
}

void ProjectPage::exit() {
}

void ProjectPage::draw(Canvas &canvas) {
    WindowPainter::clear(canvas);
    WindowPainter::drawHeader(canvas, _model, _engine, "PROJECT");
    WindowPainter::drawFooter(canvas);

    ListPage::draw(canvas);
}

void ProjectPage::updateLeds(Leds &leds) {
    ListPage::updateLeds(leds);
}

void ProjectPage::keyPress(KeyPressEvent &event) {
    const auto &key = event.key();

    if (key.isContextMenu()) {
        contextShow();
        event.consume();
        return;
    }

    if (key.pageModifier()) {
        return;
    }

    if (key.is(Key::Encoder) && selectedRow() == 0) {
        _manager.pages().textInput.show("NAME:", _project.name(), Project::NameLength, [this] (bool result, const char *text) {
            if (result) {
                _project.setName(text);
            }
        });

        return;
    }

    ListPage::keyPress(event);
}

void ProjectPage::encoder(EncoderEvent &event) {
    ListPage::encoder(event);
}

void ProjectPage::contextShow() {
    showContextMenu(ContextMenu(
        contextMenuItems,
        int(ContextAction::Last),
        [&] (int index) { contextAction(index); },
        [&] (int index) { return contextActionEnabled(index); }
    ));
}

void ProjectPage::contextAction(int index) {
    switch (ContextAction(index)) {
    case ContextAction::Init:
        initProject();
        break;
    case ContextAction::Load:
        loadProject();
        break;
    case ContextAction::Save:
        saveProject();
        break;
    case ContextAction::SaveAs:
        saveAsProject();
        break;
    case ContextAction::Route:
        initRoute();
        break;
    case ContextAction::Last:
        break;
    }
}

bool ProjectPage::contextActionEnabled(int index) const {
    switch (ContextAction(index)) {
    case ContextAction::Load:
    case ContextAction::Save:
    case ContextAction::SaveAs:
        return FileManager::volumeMounted();
    case ContextAction::Route:
        return _listModel.routingParam(selectedRow()) != Routing::Param::None;
    default:
        return true;
    }
}

void ProjectPage::initProject() {
    _manager.pages().confirmation.show("ARE YOU SURE?", [this] (bool result) {
        if (result) {
            _project.clear();
            showMessage("PROJECT INITIALIZED");
        }
    });
}

void ProjectPage::loadProject() {
    _manager.pages().fileSelect.show("LOAD PROJECT", FileType::Project, _project.slotAssigned() ? _project.slot() : 0, false, [this] (bool result, int slot) {
        if (result) {
            _manager.pages().confirmation.show("ARE YOU SURE?", [this, slot] (bool result) {
                if (result) {
                    loadProjectFromSlot(slot);
                }
            });
        }
    });
}

void ProjectPage::saveProject() {
    if (!_project.slotAssigned()) {
        saveAsProject();
        return;
    }

    saveProjectToSlot(_project.slot());
}

void ProjectPage::saveAsProject() {
    _manager.pages().fileSelect.show("SAVE PROJECT", FileType::Project, 0, true, [this] (bool result, int slot) {
        if (result) {
            if (FileManager::slotUsed(FileType::Project, slot)) {
                _manager.pages().confirmation.show("ARE YOU SURE?", [this, slot] (bool result) {
                    if (result) {
                        saveProjectToSlot(slot);
                    }
                });
            } else {
                saveProjectToSlot(slot);
            }
        }
    });
}

void ProjectPage::initRoute() {
    _manager.pages().top.editRoute(_listModel.routingParam(selectedRow()), 0);
}

void ProjectPage::saveProjectToSlot(int slot) {
    _engine.lock();
    _manager.pages().busy.show("SAVING PROJECT ...");

    FileManager::task([this, slot] () {
        return FileManager::saveProject(_project, slot);
    }, [this] (fs::Error result) {
        if (result == fs::OK) {
            showMessage(FixedStringBuilder<32>("SAVED PROJECT"));
        } else {
            showMessage(FixedStringBuilder<32>("FAILED (%s)", fs::errorToString(result)));
        }
        // TODO lock ui mutex
        _manager.pages().busy.close();
        _engine.unlock();
    });
}

void ProjectPage::loadProjectFromSlot(int slot) {
    _engine.lock();
    _manager.pages().busy.show("LOADING PROJECT ...");

    FileManager::task([this, slot] () {
        return FileManager::loadProject(_project, slot);
    }, [this] (fs::Error result) {
        if (result == fs::OK) {
            showMessage(FixedStringBuilder<32>("LOADED PROJECT"));
        } else {
            showMessage(FixedStringBuilder<32>("FAILED (%s)", fs::errorToString(result)));
        }
        // TODO lock ui mutex
        _manager.pages().busy.close();
        _engine.unlock();
    });
}
