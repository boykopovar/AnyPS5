// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/accessibility/AccessibilityObjectInterface.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

namespace NKWebKit::WebCoreAbi {

enum class AccessibilityRole : int {
    Annotation = 1,
    Application,
    ApplicationAlert,
    ApplicationAlertDialog,
    ApplicationDialog,
    ApplicationGroup,
    ApplicationLog,
    ApplicationMarquee,
    ApplicationStatus,
    ApplicationTextGroup,
    ApplicationTimer,
    Audio,
    Blockquote,
    Browser,
    BusyIndicator,
    Button,
    Canvas,
    Caption,
    Cell,
    CheckBox,
    Code,
    ColorWell,
    Column,
    ColumnHeader,
    ComboBox,
    Definition,
    Deletion,
    DescriptionList,
    DescriptionListDetail,
    DescriptionListTerm,
    Details,
    Directory,
    DisclosureTriangle,
    Document,
    DocumentArticle,
    DocumentMath,
    DocumentNote,
    Drawer,
    EditableText,
    Feed,
    Figure,
    Footer,
    Footnote,
    Form,
    Generic,
    GraphicsDocument,
    GraphicsObject,
    GraphicsSymbol,
    Grid,
    GridCell,
    Group,
    GrowArea,
    Heading,
    HelpTag,
    HorizontalRule,
    Ignored,
    Inline,
    Image,
    ImageMap,
    ImageMapLink,
    Incrementor,
    Insertion,
    Label,
    LandmarkBanner,
    LandmarkComplementary,
    LandmarkContentInfo,
    LandmarkDocRegion,
    LandmarkMain,
    LandmarkNavigation,
    LandmarkRegion,
    LandmarkSearch,
    Legend,
    Link,
    List,
    ListBox,
    ListBoxOption,
    ListItem,
    ListMarker,
    Mark,
    MathElement,
    Matte,
    Menu,
    MenuBar,
    MenuButton,
    MenuItem,
    MenuItemCheckbox,
    MenuItemRadio,
    MenuListPopup,
    MenuListOption,
    Meter,
    Model,
    Outline,
    Paragraph,
    PopUpButton,
    Pre,
    Presentational,
    ProgressIndicator,
    RadioButton,
    RadioGroup,
    RowHeader,
    Row,
    RowGroup,
    RubyBase,
    RubyBlock,
    RubyInline,
    RubyRun,
    RubyText,
    Ruler,
    RulerMarker,
    ScrollArea,
    ScrollBar,
    SearchField,
    Sheet,
    Slider,
    SliderThumb,
    SpinButton,
    SpinButtonPart,
    SplitGroup,
    Splitter,
    StaticText,
    Subscript,
    Suggestion,
    Summary,
    Superscript,
    Switch,
    SystemWide,
    SVGRoot,
    SVGText,
    SVGTSpan,
    SVGTextPath,
    TabGroup,
    TabList,
    TabPanel,
    Tab,
    Table,
    TableHeaderContainer,
    Term,
    TextArea,
    TextField,
    TextGroup,
    Time,
    Tree,
    TreeGrid,
    TreeItem,
    ToggleButton,
    Toolbar,
    Unknown,
    UserInterfaceTooltip,
    ValueIndicator,
    Video,
    WebApplication,
    WebArea,
    WebCoreLink,
    Window,
};

}  // namespace NKWebKit::WebCoreAbi

extern "C" {

bool APS5_VABI _ZN7WebCore19AccessibilityObject11isARIAInputENS_17AccessibilityRoleE(NKWebKit::WebCoreAbi::AccessibilityRole ariaRole);
bool APS5_VABI _ZN7WebCore19AccessibilityObject13isARIAControlENS_17AccessibilityRoleE(NKWebKit::WebCoreAbi::AccessibilityRole ariaRole);
NKWebKit::WebCoreAbi::AccessibilityRole APS5_VABI _ZN7WebCore19AccessibilityObject21ariaRoleToWebCoreRoleERKN3WTF6StringE(const NKWebKit::WtfAbi::String* value);

}
