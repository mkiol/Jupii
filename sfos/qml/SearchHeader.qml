import QtQuick 2.0
import Sailfish.Silica 1.0

FocusScope {
    id: root

    property alias placeholderText: searchField.placeholderText
    property alias dialog: header.dialog
    property alias searchFieldLeftMargin: searchField.textLeftMargin
    property QtObject model
    property bool singleSelectionMode: true
    readonly property bool active: searchField.text.length > 0

    implicitHeight: col.height

    Column {
        id: col
        width: parent.width

        DialogHeader {
            id: header

            spacing: 0

            /*acceptText: {
                if (root.singleSelectionMode) {
                    return dialog.acceptText.length ? dialog.acceptText : defaultAcceptText
                } else {
                    var selectedCount = model.selectedModel ? model.selectedModel.selectedCount(contentType) : 0
                    //: Multi content picker number of selected content items
                    //% "%n selected"
                    return selectedCount > 0 ? qsTrId("components_pickers-he-multiselect_accept", selectedCount) :
                                               (dialog.acceptText.length ? dialog.acceptText : defaultAcceptText)
                }
            }*/
        }

        SearchField {
            id: searchField
            width: parent.width
        }

        Binding {
            target: model
            property: "filter"
            value: searchField.text.toLowerCase().trim()
        }
    }
}
