User Interface
==============

Widgets
-------

Apart from the main display, SpeedCrunch offers a number of extra panels, referred to as *widgets* here.
They can be enabled and disabled via the :menuselection:`View` menu.

* Formula Book
    The formula book provides access to commonly used formulas and calculations. Simply insert
    a formula into the expression editor by clicking on it.

    You can help expanding the formula book by posting your requests to the `issue tracker <tracker_>`_.

* Constants
    The constants widget shows a list of over 150 scientific constants. Just double-click on an entry
    to paste it into the editor.

    .. note::
       As of version 0.12, the constants have not yet been adapted to make use of the new unit system.
       All the inserted values thus lack a unit. This is likely to change in a future version.

* Variables
    The variables widget lists all :ref:`user-defined variables <variables>`. Any of them can be inserted into the editor by double-clicking it.
    Additionally, it is possible to delete a variable by selecting it and pressing the :kbd:`Delete` key on your keyboard.

* Functions and User Functions
    Similar to the variables widget, these show built-in and :ref:`user-defined functions <user_functions>` respectively.

* Bit Field
    The bit field widget is designed to make working with binary numbers easier. It shows a field of 64 squares,
    each representing a bit in the current result. Any bit can be toggled by clicking its square; the resulting
    number is automatically inserted into the editor. Additional buttons allow you to shift, invert and reset all the bits at once.
    While the mouse cursor is hovering over the bit field, scrolling the mouse wheel will also shift the bits.

  .. _keypad:
* Keypad
    The on-screen keypad allows inputting numbers without using the keyboard. However, it is very limited and doesn't provide
    access to many of SpeedCrunch's more advanced features. For that reason, using SpeedCrunch's keyboard interface is recommended
    in place of using the keypad.

    Available entries in :menuselection:`View --> Keypad` are:

    * ``Basic`` (**default**)
    * ``Scientific (wide)``
    * ``Scientific (narrow)``
    * ``Custom...``
    * ``Disabled``

    ``Custom...`` opens a dialog where you can define the keypad matrix size (rows and columns)
    and configure each button individually. For each position in the matrix, you can set:

    * The button label shown in the keypad
    * The button behavior: ``Insert text``, ``Backspace``, ``Clear expression``, or ``Evaluate expression``
    * The text to insert (for ``Insert text`` behavior)

    Custom keypad settings are saved and restored automatically.

    .. versionchanged:: 0.11
       The keypad was removed in SpeedCrunch 0.11; however, it was added back in 0.12.

* History
    The history widget lists all previous inputs. Double-click a line to recall it. Note that the main result display also provides this functionality.

.. _tracker: https://bitbucket.org/heldercorreia/speedcrunch/issues


Expression Editor Features
--------------------------

The expression editor provides some advanced features:

* Autocompletion
    If you start typing a name (e.g. of a variable, function, or unit), a pop-up with matching names will appear. Pressing :kbd:`Tab` or :kbd:`Enter`
    will automatically insert the first suggestion. Alternatively, you can use the arrow keys or the mouse to select a different suggestion, or continue
    typing to refine the list.

* Quick constant insertion
    Press :kbd:`Control+Space` to open a list of constants that allows quick access to the same constants as the constants widget (see above).
    Use the arrow keys to navigate the list.

  .. _context-help:
* Context help
    Pressing :kbd:`F1` will show the manual page for the function under the cursor, providing quick access to detailed
    usage information for a function. Pressing :kbd:`Escape` will dismiss the manual window again.

* Selection results
    If :menuselection:`Settings --> Results --> Show Live Result Preview` is enabled, selecting a partial expression in the expression editor will show
    you the result of the selected expression.


Import/Export
-------------

SpeedCrunch can save/export your session in a number of ways. The :menuselection:`Session --> Save` and :menuselection:`Session --> Load` menu entries
allow you to easily save and restore your entire SpeedCrunch session. The data is stored in a SpeedCrunch-specific file format. [#f1]_
While the session files are human-readable, they are designed for use by SpeedCrunch. If you want to export your
calculations to work on them in another program or hand them to a colleague, the other export options are preferable.

You can save the session as HTML (:menuselection:`Session --> Export --> HTML`). The resulting file will consist of the contents of the result
display and can be viewed in any web browser. This feature can also be used to print a SpeedCrunch session by printing the exported
HTML document. Since the syntax highlighting and color scheme are maintained in the HTML output, it is recommended to select a color scheme
with a white background (e.g. *Standard*) prior to exporting if you intend to print the document.

The final, most basic option is to export your session as a plain text file (:menuselection:`Session --> Export --> Plain text`).
In contrast to the HTML export option, the syntax highlighting will be lost.

SpeedCrunch also offers capabilities to *import* a session from a text file (:menuselection:`Session --> Import`).
Select any plain text file and SpeedCrunch will try to evaluate each line of the file as if the user entered it directly.

Startup User Definitions
++++++++++++++++++++++++

To define user variables and user functions that are loaded automatically at startup, use
:menuselection:`Session --> Startup User Functions and Variables...`.

This dialog provides:

* A multi-line editor (one definition per line), with syntax highlighting and line numbers.
* Merge/overwrite behavior selection for name collisions.
* A warning banner when overwrite mode is selected.
* :guilabel:`Apply Now` to apply definitions immediately.
* :guilabel:`Test Now` to validate and preview results without applying.
* :guilabel:`Import...` / :guilabel:`Export...` to share startup definitions as plain text (``.txt``).
* An advanced option to apply definitions before session restore (instead of after restore).

When applying or testing, SpeedCrunch reports:

* Imported variable count
* Imported function count
* Line numbers with errors

Invalid definitions are ignored. Variable definitions that evaluate to ``NaN`` are also ignored.


Settings
--------

SpeedCrunch's behavior can be customized to a large degree using the configuration options in the
:menuselection:`Settings` menu. This section explains the settings that are available.

.. _result_format:

Format
+++++++++++++

This section allows selecting the result format to use. You can select one of the following
formats:

* :menuselection:`Decimal --> Automatic`
    Use fixed-point decimal form for most results; for very large (more than six integer places) or very small results (less than 0.0001),
    scientific notation will be used.
* :menuselection:`Decimal --> Fixed-Point`
    Display results in fixed-point decimal form. For excessively
    large or small numbers, this format may still fall back to scientific notation.
* :menuselection:`Decimal --> Engineering`
    Display results in engineering notation. This is a variant of :ref:`scientific notation <scientific_notation>` in which
    the exponent is divisible by three.
* :menuselection:`Decimal --> Scientific`
    Display results in :ref:`normalized scientific notation <scientific_notation>`.
* :menuselection:`Binary`
    Display results as binary numbers, i.e. in base-2.
* :menuselection:`Octal`
    Display all results as octal numbers, i.e. in base-8.
* :menuselection:`Hexadecimal`
    Display all results as hexadecimal numbers, i.e. in base-16.
* :menuselection:`Sexagecimal`
    Display dimensionless and time results as :ref:`sexagecimal values <sexagecimal_values>`, i.e. with minutes and seconds. All other results are displayed in fixed-point decimal form.

.. _radix_character:

Angle Unit
++++++++++

Select the angular unit to be used in calculations. For functions that operate on angles, notably the
:ref:`trigonometric functions <trigonometric>` like :func:`sin` or :func:`cos`, this setting
determines the angle format of the arguments.

* :menuselection:`Radian`
    Use radians for angles. A full circle corresponds to an angle of 2π radians.
* :menuselection:`Degree`
    Use degrees for angles. A full circle corresponds to an angle of 360°.
* :menuselection:`Gradian`
    Use gradians for angles. A full circle corresponds to an angle of 400 gradians.

    .. versionadded:: 1.0
    
* :menuselection:`Cycle Unit`
    Cycle unit selection between Radian, Degree and Gradian.

Digit Grouping
++++++++++++++

Visually group digits in long numbers. Requires
:menuselection:`Settings --> Appearance --> Syntax Highlighting` to be enabled.

* :menuselection:`Digit Grouping`
    Visually group digits in long numbers.
* :menuselection:`Digit Grouping --> Group Integer Part Only`
    Apply digit grouping only to the integer part of numbers. When enabled, fractional digits remain ungrouped.

History
+++++++

This section contains settings that control how calculation history is stored.

* :menuselection:`History Saving --> Never`
    Do not persist the calculation history between runs.
* :menuselection:`History Saving --> On Exit`
    Save the calculation history when SpeedCrunch exits and restore it on the next launch.
* :menuselection:`History Saving --> Continuously`
    Save calculation history after each new calculation and restore it on the next launch.
* :menuselection:`History Saving`
    This preference only affects calculation history. User-defined functions and user-defined
    variables are always persisted.
* :menuselection:`History Size Limit...`
    Sets the maximum number of stored history entries. By default, SpeedCrunch keeps
    up to 100 entries and automatically removes the oldest ones when this limit is
    exceeded. Set the value to ``0`` to disable the limit.


Window
++++++

This section contains settings that control the main window behavior.

* :menuselection:`Save Window Position on Exit`
    Controls if the window position is saved and restored.
* :menuselection:`Single Instance`
    When enabled (default), launching SpeedCrunch while it is already running focuses
    the existing window instead of starting a second instance.
* :menuselection:`Always on Top`
    Keep the SpeedCrunch window on top of other windows.


Results
+++++++

This section contains settings that control result output and post-evaluation behavior.

* :menuselection:`Show Live Result Preview`
    If set, SpeedCrunch will display partial results as you type your expression as well
    as results when selecting a partial expression in the editor.

* :menuselection:`Format`
    Select the format used to display results.
* :menuselection:`Precision`
    Select the number of fractional digits to display.
    **Automatic** always displays as many digits as are necessary to represent the number
    precisely. The preset settings and :menuselection:`Custom...` explicitly specify a
    certain number of digits (from 0 to 50) and will append additional zeroes to the
    fraction to reach that number of digits, if necessary.
* :menuselection:`Complex Numbers --> Disabled`, :menuselection:`Complex Numbers --> Cartesian`, and :menuselection:`Complex Numbers --> Polar`
    Select the complex-number mode. ``Disabled`` turns off support for :ref:`complex numbers <complex_numbers>` (so
    :const:`j` is undefined and expressions such as ``sqrt(-1)`` fail). ``Cartesian`` and ``Polar`` both enable
    complex-number support and choose how complex results are displayed. ``Disabled`` is the default.
* :menuselection:`Secondary Format` and :menuselection:`Tertiary Format`
    Optional extra result displays in alternate formats, shown alongside the primary result.
* :menuselection:`Automatically Copy New Results to Clipboard`
    Automatically copy each newly evaluated result to the clipboard.


Editing
+++++++

* :menuselection:`Automatic Completion`
    Completely enables or disables autocompletion.

.. _automatic_result_reuse:

* :menuselection:`Auto-Insert "ans" When Starting with an Operator`
    If a new expression starts with ``+``, ``-``, ``*``, or ``/``, SpeedCrunch inserts ``ans`` first.
* :menuselection:`Show Empty History Hint`
    Show or hide the ``Type an expression here`` hint when there are no calculations in history.
* :menuselection:`Keep Entered Expression After Evaluate`
    If selected, the entered expression remains in the editor after evaluating it.
* :menuselection:`Radix Character`
    Select the decimal separator to use in inputs and results. This can either be explicitly set
    to dot (``.``), or comma (``,``), or both, or system default. When both dot and comma are used,
    the decimal separator is detected automatically in inputs and the system default is used
    in results. With that latter mode, mixing both dot and comma in a same number to express the
    decimal separator and digit group separators is supported, but might lead to unexpected results.
    See :ref:`Digit Grouping Separators <digit_grouping_separators>` for allowed grouping characters
    and examples.


User Interface Settings
+++++++++++++++++++++++

* :menuselection:`Settings --> Appearance --> Theme`
    Select a theme. See :ref:`color_schemes` for information on how to install
    additional themes so they are displayed in this menu.
* :menuselection:`Settings --> Appearance --> Font`
    Select the font to use for the expression editor and result display.
* :menuselection:`Settings --> Appearance --> Syntax Highlighting`
    Enable or disable syntax highlighting.
* :menuselection:`Settings --> Appearance --> Hover Highlighting`
    Enable or disable hover highlighting.
* :menuselection:`Settings --> Language`
    Select the user interface language.


Keyboard Shortcuts
------------------

Editing
+++++++
* :kbd:`Control+L`
    Load session.
* :kbd:`Control+S`
    Save session.
* :kbd:`Control+Q`
    Quit SpeedCrunch.
* :kbd:`Control+N`
    Clear history.
* :kbd:`Control+C`
    Copy selected text to clipboard.
* :kbd:`Control+R`
    Copy last result to clipboard.
* :kbd:`Control+V`
    Paste from clipboard.
* :kbd:`Control+A`
    Select entire expression.
* :kbd:`Control+P`
    Wrap the current selection in parentheses. If no text is selected, the entire expression is wrapped.

Widgets and Docks
+++++++++++++++++

* :kbd:`Control+1`
    Show/hide formula book.
* :kbd:`Control+2`
    Show/hide constants widget.
* :kbd:`Control+3`
    Show/hide functions widgets.
* :kbd:`Control+4`
    Show/hide variables widget.
* :kbd:`Control+5`
    Show/hide user functions widget.
* :kbd:`Control+6`
    Show/hide bit field widget.
* :kbd:`Control+7`
    Show/hide history widget.
* :kbd:`Control+B`
    Show/hide the status bar.

Scrolling
+++++++++

* :kbd:`Page Up` and :kbd:`Page Down`
    Scroll the result window page-wise.
* :kbd:`Shift+Page Up` and :kbd:`Shift+Page Down`
    Scroll the result window line-wise.
* :kbd:`Control+Page Up` and :kbd:`Control+Page Down`
    Scroll to the top or bottom of the result window.


Format
++++++

* :kbd:`F2`
    Set result format to automatic decimal.
* :kbd:`F3`
    Set result format to fixed-point decimal.
* :kbd:`F4`
    Set result format to engineering decimal.
* :kbd:`F5`
    Set result format to scientific decimal.
* :kbd:`F6`
    Set result format to binary.
* :kbd:`F7`
    Set result format to octal.
* :kbd:`F8`
    Set result format to hexadecimal.
* :kbd:`F9`
    Set result format to sexagecimal.

    .. versionadded:: 1.0
    
* :kbd:`F10`
    Cycle angle unit (Degree/Radian/Gradian).
* :kbd:`Control+.`
    Use a period as decimal separator.
* :kbd:`Control+,`
    Use a comma as decimal separator.

Various
+++++++

* :kbd:`F1`
    Show context help (dismiss with :kbd:`Escape`).

    .. versionadded:: 0.12

* :kbd:`F11`
    Toggle full screen.
* :kbd:`Control` + mouse wheel, :kbd:`Shift` + mouse wheel, or :kbd:`Shift+Up` and :kbd:`Shift+Down`
    Change the font size.
* :kbd:`Control+Shift` + mouse wheel
    Change the window opacity.

    .. versionadded:: 0.12


.. rubric:: Footnotes

.. [#f1] Starting with SpeedCrunch 0.12, the session format is based on `JSON <json_>`_. Previous
         versions used a simple custom text format.

.. _json: http://json.org/
