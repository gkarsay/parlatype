---
layout: page
title: How to check signed sources
---

## Verify signature
In the download section you can find a tarball and a signature. The signature has the same name as the tarball with `.asc` appended.

The signing key is
```
pub   dsa1024 2009-11-14 [SC]
      ECD6 1821 2C4D 02A7 2BB3  7406 5A9D 5CEF 71B0 9FC3
uid           [ultimate] Gabor Karsay <gabor.karsay@gmx.at>
sub   elg2048 2011-08-20 [E]
```

The public block can be downloaded here: [signing-key.asc](signing-key.asc)



To verify the signature, use this command, replacing [signature] and [tarball].
```
gpg --verify [signature] [tarball]
```
For example
```
gpg --verify parlatype-1.5.6.tar.gz.asc parlatype-1.5.6.tar.gz
```

The result should look like this:
```
gpg: Signature made Fre 17 Aug 2018 12:50:42 CEST
gpg:                using DSA key ECD618212C4D02A72BB374065A9D5CEF71B09FC3
gpg: Good signature from "Gabor Karsay <gabor.karsay@gmx.at>" [ultimate]
```
