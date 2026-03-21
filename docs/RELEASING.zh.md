# 发版

[English](RELEASING.md) | 中文

改 `versionName` / `versionCode`，写 `CHANGELOG`，打 tag `v*`，三者一致。

密钥：`trierarch-app/` 里 `keytool` 建 `release.keystore`，`keystore.properties.example` 复制成 `keystore.properties` 填好。密钥文件别提交。

```bash
cd trierarch-app && ./gradlew assembleRelease
```

APK 在 `app/build/outputs/apk/release/`。要发就挂 GitHub Release，可选 `sha256sum` 贴说明里。

没配 `keystore.properties` 时 release 会用 debug 签，仅适合自己测。

tag 打过了以后又有新提交：照常 commit，下版再打新 tag，一般别改已推送的旧 tag。
