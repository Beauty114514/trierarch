# Release

[中文](RELEASING.zh.md) | English

Bump `versionName` / `versionCode`, update `CHANGELOG`, tag `v*`, keep them in line.

Keystore: create `release.keystore` under `trierarch-app/` with `keytool`, copy `keystore.properties.example` to `keystore.properties`. Don’t commit secrets.

```bash
cd trierarch-app && ./gradlew assembleRelease
```

APK: `app/build/outputs/apk/release/`. Upload on GitHub Releases; optional `sha256sum` in the notes.

Without `keystore.properties`, release is signed with the debug key—fine for local testing only.

After a tag, more commits are normal: commit as usual, tag again on the next release; don’t rewrite pushed tags unless you know why.
