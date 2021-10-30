---
layout: page
title: How to check signed sources
---

# Verify signature
In the download section you can find a tarball and a signature. The signature has the same name as the tarball with `.asc` appended.


## Current signing key (since Parlatype 3.0)

```
pub   rsa4096 2020-12-12 [SC] [expires: 2022-12-12]
      F8BB E845 C969 F0AD 61AF  8D07 C892 C2F1 310F 600B
uid           [ultimate] Gabor Karsay <gabor.karsay@gmx.at>
sub   rsa4096 2020-12-12 [E] [expires: 2022-12-12]
```
The public block can be downloaded here: [signing-key.asc](signing-key.asc)



To verify the signature, use this command, replacing [signature] and [tarball].
```
gpg --verify [signature] [tarball]
```
For example
```
gpg --verify parlatype-3.0.tar.gz.asc parlatype-3.0.tar.gz
```

The result should look like this:
```
gpg: Signature made Sa 30 Okt 2021 18:04:35 CEST
gpg:                using RSA key F8BBE845C969F0AD61AF8D07C892C2F1310F600B
gpg: Good signature from "Gabor Karsay <gabor.karsay@gmx.at>" [ultimate]
```

## Old signing key (until Parlatype 2.1)

```
pub   dsa1024 2009-11-14 [SC]
      ECD6 1821 2C4D 02A7 2BB3  7406 5A9D 5CEF 71B0 9FC3
uid           [ultimate] Gabor Karsay <gabor.karsay@gmx.at>
sub   elg2048 2011-08-20 [E]
```

The public block can be downloaded here: [signing-key-old.asc](signing-key-old.asc)


The verification should look like this:
```
gpg: Signature made Fre 17 Aug 2018 12:50:42 CEST
gpg:                using DSA key ECD618212C4D02A72BB374065A9D5CEF71B09FC3
gpg: Good signature from "Gabor Karsay <gabor.karsay@gmx.at>" [ultimate]
```
