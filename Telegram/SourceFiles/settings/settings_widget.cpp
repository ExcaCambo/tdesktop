/*
This file is part of Telegram Desktop,
the official desktop version of Telegram messaging app, see https://telegram.org

Telegram Desktop is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

In addition, as a special exception, the copyright holders give permission
to link the code of portions of this program with the OpenSSL library.

Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
Copyright (c) 2014-2017 John Preston, https://desktop.telegram.org
*/
#include "stdafx.h"
#include "settings/settings_widget.h"

#include "settings/settings_inner_widget.h"
#include "settings/settings_fixed_bar.h"
#include "styles/style_settings.h"
#include "styles/style_window.h"
#include "styles/style_boxes.h"
#include "ui/effects/widget_fade_wrap.h"
#include "ui/widgets/scroll_area.h"
#include "ui/widgets/buttons.h"
#include "mainwindow.h"
#include "mainwidget.h"
#include "localstorage.h"
#include "boxes/confirmbox.h"
#include "lang.h"
#include "application.h"
#include "ui/filedialog.h"
#include "window/themes/window_theme.h"
#include "window/themes/window_theme_editor.h"

namespace Settings {
namespace {

QString SecretText;
QMap<QString, base::lambda_copy<void()>> Codes;

void fillCodes() {
	Codes.insert(qsl("debugmode"), []() {
		QString text = cDebug() ? qsl("Do you want to disable DEBUG logs?") : qsl("Do you want to enable DEBUG logs?\n\nAll network events will be logged.");
		Ui::show(Box<ConfirmBox>(text, [] {
			App::app()->onSwitchDebugMode();
		}));
	});
	Codes.insert(qsl("testmode"), []() {
		auto text = cTestMode() ? qsl("Do you want to disable TEST mode?") : qsl("Do you want to enable TEST mode?\n\nYou will be switched to test cloud.");
		Ui::show(Box<ConfirmBox>(text, [] {
			App::app()->onSwitchTestMode();
		}));
	});
	Codes.insert(qsl("loadlang"), []() {
		Global::RefChooseCustomLang().notify();
	});
	Codes.insert(qsl("debugfiles"), []() {
		if (!cDebug()) return;
		if (DebugLogging::FileLoader()) {
			Global::RefDebugLoggingFlags() &= ~DebugLogging::FileLoaderFlag;
		} else {
			Global::RefDebugLoggingFlags() |= DebugLogging::FileLoaderFlag;
		}
		Ui::show(Box<InformBox>(DebugLogging::FileLoader() ? qsl("Enabled file download logging") : qsl("Disabled file download logging")));
	});
	Codes.insert(qsl("crashplease"), []() {
		t_assert(!"Crashed in Settings!");
	});
	Codes.insert(qsl("workmode"), []() {
		auto text = Global::DialogsModeEnabled() ? qsl("Disable work mode?") : qsl("Enable work mode?");
		Ui::show(Box<ConfirmBox>(text, [] {
			App::app()->onSwitchWorkMode();
		}));
	});
	Codes.insert(qsl("moderate"), []() {
		auto text = Global::ModerateModeEnabled() ? qsl("Disable moderate mode?") : qsl("Enable moderate mode?");
		Ui::show(Box<ConfirmBox>(text, []() {
			Global::SetModerateModeEnabled(!Global::ModerateModeEnabled());
			Local::writeUserSettings();
			Ui::hideLayer();
		}));
	});
	Codes.insert(qsl("getdifference"), []() {
		if (auto main = App::main()) {
			main->getDifference();
		}
	});
	Codes.insert(qsl("loadcolors"), []() {
		FileDialog::askOpenPath("Open palette file", "Palette (*.tdesktop-palette)", [](const FileDialog::OpenResult &result) {
			if (!result.paths.isEmpty()) {
				Window::Theme::Apply(result.paths.front());
			}
		});
	});
	Codes.insert(qsl("edittheme"), []() {
		Window::Theme::Editor::Start();
	});
	Codes.insert(qsl("videoplayer"), []() {
		auto text = cUseExternalVideoPlayer() ? qsl("Use internal video player?") : qsl("Use external video player?");
		Ui::show(Box<ConfirmBox>(text, [] {
			cSetUseExternalVideoPlayer(!cUseExternalVideoPlayer());
			Local::writeUserSettings();
			Ui::hideLayer();
		}));
	});
}

void codesFeedString(const QString &text) {
	if (Codes.isEmpty()) fillCodes();

	SecretText += text.toLower();
	int size = SecretText.size(), from = 0;
	while (size > from) {
		auto piece = SecretText.midRef(from);
		auto found = false;
		for (auto i = Codes.cbegin(), e = Codes.cend(); i != e; ++i) {
			if (piece == i.key()) {
				(*i)();
				from = size;
				found = true;
				break;
			}
		}
		if (found) break;

		for (auto i = Codes.cbegin(), e = Codes.cend(); i != e; ++i) {
			if (i.key().startsWith(piece)) {
				found = true;
				break;
			}
		}
		if (found) break;

		++from;
	}
	SecretText = (size > from) ? SecretText.mid(from) : QString();
}

} // namespace

Widget::Widget(QWidget *parent) {
	setTitle(lang(lng_menu_settings));
	_inner = setInnerWidget(object_ptr<InnerWidget>(this));
	setCloseClickHandler([]() {
		Ui::hideSettingsAndLayer();
	});
}

void Widget::showFinished() {
	_inner->showFinished();
}

void Widget::keyPressEvent(QKeyEvent *e) {
	codesFeedString(e->text());
	return LayerWidget::keyPressEvent(e);
}

void Widget::parentResized() {
	auto parentSize = parentWidget()->size();
	auto windowWidth = parentSize.width();
	auto newWidth = st::settingsMaxWidth;
	auto newContentLeft = st::settingsMaxPadding;
	if (windowWidth <= st::settingsMaxWidth) {
		newWidth = windowWidth;
		newContentLeft = st::settingsMinPadding;
		if (windowWidth > st::windowMinWidth) {
			// Width changes from st::windowMinWidth to st::settingsMaxWidth.
			// Padding changes from st::settingsMinPadding to st::settingsMaxPadding.
			newContentLeft += ((newWidth - st::windowMinWidth) * (st::settingsMaxPadding - st::settingsMinPadding)) / (st::settingsMaxWidth - st::windowMinWidth);
		}
	} else if (windowWidth < st::settingsMaxWidth + 2 * st::settingsMargin) {
		newWidth = windowWidth - 2 * st::settingsMargin;
		newContentLeft = st::settingsMinPadding;
		if (windowWidth > st::windowMinWidth) {
			// Width changes from st::windowMinWidth to st::settingsMaxWidth.
			// Padding changes from st::settingsMinPadding to st::settingsMaxPadding.
			newContentLeft += ((newWidth - st::windowMinWidth) * (st::settingsMaxPadding - st::settingsMinPadding)) / (st::settingsMaxWidth - st::windowMinWidth);
		}
	}
	resizeToWidth(newWidth, newContentLeft);
}

void Widget::resizeUsingInnerHeight(int newWidth, int innerHeight) {
	if (!App::wnd()) return;

	auto parentSize = parentWidget()->size();
	auto windowWidth = parentSize.width();
	auto windowHeight = parentSize.height();
	auto maxHeight = st::settingsFixedBarHeight + innerHeight;
	auto newHeight = maxHeight + st::boxRadius;
	if (newHeight > windowHeight || newWidth >= windowWidth) {
		newHeight = windowHeight;
	}

	auto roundedCorners = newHeight < windowHeight;
	setRoundedCorners(roundedCorners);
	setAttribute(Qt::WA_OpaquePaintEvent, !roundedCorners);

	setGeometry((windowWidth - newWidth) / 2, (windowHeight - newHeight) / 2, newWidth, newHeight);
	update();
}

} // namespace Settings
