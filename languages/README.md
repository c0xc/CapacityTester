TRANSLATIONS
------------

For each translation, there's a ts file in the `languages/` directory.
It contains the original strings and, after it's been translated,
their respective translations as well.
Those ts files may be created by the developer whenever a language is added.
Once a translation is complete, it's added to the `TRANSLATIONS` list
in the project file and to the resource file `res/lang.qrc`.
The translated ts file is then converted and committed.

Normally, the locale / ISO 639 language code is used as suffix:
`capacitytester_de.qm`

It's also possible to use the language as suffix:
`capacitytester_german.qm`

The program determines the configured language automatically.
To use a different language, use the LANGUAGE variable:

    LANGUAGE=de ./bin/CapacityTester

Or: `i18n=German`



