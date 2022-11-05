TRANSLATIONS
============

If you want to help translate this program into another language,
make a pull request with an updated ts file
or open an issue and ask.

The procedure is as follows.

For each language, there's a ts file in the `languages/` directory,
for example "capacitytester_en.ts" (English) or "capacitytester_es.ts" (Spanish).
This file contains all the texts that can be translated.
It can be opened and edited with the Qt Linguist program.

Even though all the untranslated texts are in English,
there is an English translation file to handle plural forms.
As shown in the following screenshot, the text "%n file(s) have been found"
requires two translations, one singular form "file has been found"
(where "%n" is 1) and one plural form "files have been found".
For languages with two different plural forms, three translations would
be required for this text.

![English translation (Linguist)](screenshots/Linguist_en_1.png)

This screenshot shows the file "capacitytester_de.ts"
for the German translation. If the text has no number in it,
simply type in the translated text and confirm to continue with the next one.

![German translation (Linguist)](screenshots/Linguist_de_1.png)

When all text strings are translated (all having the green check mark),
save the file and send it in by opening a pull request.



Internals
---------

If the translation files are incomplete and the developer forgot
to update them, they can be updated using the `lupdate` tool:

    $ lupdate capacitytester.pro



Thanks
------

* Spanish translation by @caralu74 (issue #3)



