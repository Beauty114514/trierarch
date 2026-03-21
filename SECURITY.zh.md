# 安全策略

[English](SECURITY.md) | 中文

## 如何报告漏洞

**请勿**在公开 GitHub Issue 中披露**尚未修复**的安全问题（会在修复前公之于众）。

### 优先：GitHub 私密报告

若仓库已开启 **[私下报告漏洞](https://docs.github.com/zh/code-security/security-advisories/guidance-on-reporting-and-writing-information-about-vulnerabilities/privately-reporting-a-security-vulnerability)**，请在 GitHub 仓库页使用 **Security → Report a vulnerability**。

### 备选：维护者私下联系

若无法使用上述功能，请通过维护者公布的**非公开**渠道联系（例如主页邮箱等）。**不要**在公开 issue 或社交平台贴利用细节。

## 请尽量提供

- 影响的**组件**（应用、native、proot、wayland 等）及**版本 / commit**（若已知）。
- **复现步骤**或清晰的影响说明。
- 你是否愿意在修复后**配合披露时间**。

## 范围

本策略适用于**本仓库**及随仓库发布的集成方式。若问题出在 **上游**（Termux proot、KDE、Firefox 等），请同时按上游策略报告；我们仍可修复**本项目的集成层**问题。

## 回复

维护者会尽量在合理时间内**确认**收到严重报告；具体节奏视精力而定。感谢协助保护用户安全。
