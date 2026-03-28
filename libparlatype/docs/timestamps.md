Title: Timestamps
SPDX-License-Identifier: GPL-3.0-or-later
SPDX-FileCopyrightText: 2026 Gabor Karsay

# Timestamps

Timestamps enable the user to reference or to jump to certain positions
in the audio file.

## Internal timestamp format

Parlatype uses an `int` for timestamps, the unit are milliseconds, e.g.
60000 for a minute. -1 represents an unknown time. This is used internally
and also saved in Parlatype's file format.

## Timestamp representation for users

For time labels or time buttons shown to users there is a string representation
that can be customized through settings.

To change between the internal format and the string representation use these functions:

- [func@timestamp_to_string]
- [func@timestamp_from_string]

They can also be used to check for formal validity, returning -1 or NULL for invalid input.

There are several methods in #PtPlayer to get timestamps, go to timestamps or to
check them against the currently loaded audio file (to ensure they don't point beyond the duration
of the file), e.g.:

- [method@Player.get_timestamp]
- [method@Player.goto_timestamp]
