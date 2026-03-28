Title: File Format
SPDX-License-Identifier: GPL-3.0-or-later
SPDX-FileCopyrightText: 2026 Gabor Karsay

# File Format

## OpenDocument-like format

Parlatype uses its own file format which is an OpenDocument-like format with the MIME type `application/vnd.oasis.opendocument.text`.
It consists of a single uncompressed XML file, also known as Flat XML, with the extension `.pt`.

See the Wikipedia entry on [OpenDocument technical specification](https://en.wikipedia.org/wiki/OpenDocument_technical_specification).

Documents produced by Parlatype should be readable with an OpenDocument consumer,
but Parlatype doesn't claim or even aspire to be a conformant OpenDocument producer.

The tag `<meta:generator>` contains the string `parlatype/` and a version identifier.

## What's different

Actually almost everything: Parlatype supports only plain text, no formatting whatsoever or other things than text.
It also doesn't have the concept of pages, everything is in a single text view.

### Linked audio file

A tag called `<meta:user-defined meta:name="Parlatype">` holds the URL to the audio file that is transcribed or described in the text file.
Note that Parlatype only reads URLs starting with `file:///`.
When a text file containing this tag is opened, the audio file is opened as well.

### Timestamps

Timestamps are marked like this: `<text:span text:style-name="pt-timestamp">3180</text:span>`.
So in other OpenDocument readers they will be shown as plain text, while in Parlatype
they are converted to a [class@Pt.TimestampButton] with user-friendly timestamp formatting.
