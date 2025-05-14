# `VectorTableDelegate` Module

## Feature Overview

The `VectorTableDelegate` class is a custom `QStyledItemDelegate` designed for use with a `QTableView` (or similar view) in the VecEdit application. It provides specialized editing and display capabilities for different columns of a vector table, ensuring a user-friendly experience and adherence to specific input constraints.

Key features include:

- Custom editors (e.g., `QLineEdit`, `QComboBox`) for specific columns.
- Input validation (e.g., character limits, allowed character sets).
- Dropdown lists populated with dynamic or static data for certain columns.
- Direct checkbox rendering and interaction within cells for boolean-type data (e.g., the 'ExT' column).

## Usage Instructions

To use `VectorTableDelegate`:

1. **Include the Header**: Include `VectorTableDelegate.h` in the file where your `QTableView` is set up.

    ```cpp
    #include "delegates/VectorTableDelegate.h" // Adjust path as necessary
    ```

2. **Instantiate the Delegate**:

    ```cpp
    VectorTableDelegate *vectorDelegate = new VectorTableDelegate(this); // Or pass the view as parent
    ```

3. **Set Options for Dropdowns**:
    Before assigning the delegate to a view, or when the options change, provide the lists for dropdowns. These lists are typically fetched from a data source (e.g., database or configuration files).

    ```cpp
    QStringList instructionItems = dataAccess->getInstructionOptions(); // Example
    QStringList timesetItems = dataAccess->getTimeSetOptions();       // Example
    QStringList captureItems = {"", "Y", "N"}; // Example for Capture

    vectorDelegate->setInstructionOptions(instructionItems);
    vectorDelegate->setTimeSetOptions(timesetItems);
    vectorDelegate->setCaptureOptions(captureItems);
    ```

4. **Assign to View**:
    Set the delegate for your `QTableView` instance. You can set it as a general delegate for the entire table, or for specific columns/rows if needed (though this delegate is designed to handle multiple column types internally based on column index).

    ```cpp
    yourTableView->setItemDelegate(vectorDelegate);
    ```

5. **Ensure Model Compatibility**:
    The underlying data model used with the `QTableView` must provide and accept data in appropriate formats for each column:
    - **Label, Comment, Instruction, TimeSet, Capture**: `QString` for display and editing.
    - **ExT**: `bool` (or a type convertible to `bool`) for display (`Qt::DisplayRole`) and editing (`Qt::EditRole`). The delegate will toggle this boolean value.

    The delegate uses the `ColumnIndex` enum (`VectorTableDelegate::LabelColumn`, `VectorTableDelegate::InstructionColumn`, etc.) internally to differentiate columns. Ensure your table view's column order matches this enum or adjust the enum/logic accordingly.

## Input/Output Specification

- **Inputs to Delegate Methods**:
  - `createEditor()`: `QModelIndex` (to determine column), `QStyleOptionViewItem`.
  - `setEditorData()`: `QWidget*` (editor), `QModelIndex` (data from model).
  - `setModelData()`: `QWidget*` (editor), `QAbstractItemModel*` (model to update), `QModelIndex`.
  - `paint()`: `QPainter*`, `QStyleOptionViewItem`, `QModelIndex`.
  - `editorEvent()`: `QEvent*`, `QAbstractItemModel*`, `QStyleOptionViewItem`, `QModelIndex`.
  - `setInstructionOptions()`, `setTimeSetOptions()`, `setCaptureOptions()`: `const QStringList&` containing items for the respective comboboxes.
- **Outputs (Interactions with the Model)**:
  - `setModelData()`: Writes data back to the model using `model->setData(index, value, Qt::EditRole)`.
    - `Label`, `Comment`, `Instruction`, `TimeSet`, `Capture`: `QString` value.
    - `ExT`: `bool` value.
  - `editorEvent()` (for `ExT` column): Directly calls `model->setData(index, !currentValue, Qt::EditRole)`.
- **Data Types Handled by Column**:
  - `LabelColumn`: `QString`. Editor: `QLineEdit` with max 16 English chars validation.
  - `InstructionColumn`: `QString`. Editor: `QComboBox` populated by `m_instructionOptions`.
  - `TimeSetColumn`: `QString`. Editor: `QComboBox` populated by `m_timeSetOptions`.
  - `CaptureColumn`: `QString`. Editor: `QComboBox` populated by `m_captureOptions` (default: "", "Y", "N").
  - `ExTColumn`: `bool`. No pop-up editor. Painted as a checkbox, toggled on click.
  - `CommentColumn`: `QString`. Editor: `QLineEdit`.

## Backward Compatibility

This module is a new UI component and does not directly handle file formats or data persistence versioning itself. Its compatibility is tied to the data provided by the model and the expected UI behavior. As long as the model provides data in the formats specified above and the view uses it as intended, it should function correctly.

## Versioning and Change History

- **Version**: 1.0.0
- **Date**: 2024-07-27
- **Changes**:
  - Initial implementation of `VectorTableDelegate`.
  - Added support for `Label`, `Instruction`, `TimeSet`, `Capture`, `ExT`, and `Comment` columns.
  - Implemented custom editors: `QLineEdit` with validation, `QComboBox` for selections.
  - Implemented custom painting and event handling for in-cell checkbox (`ExT` column).
  - Added `qDebug()` logs for key operations.
