# FlutterTap

Módulo Zygisk (Magisk / KernelSU / APatch) que redireciona o tráfego de rede de apps Flutter
selecionados para um proxy configurável e contorna a verificação de certificado TLS, para fins
de análise de tráfego. Inclui um app gerenciador (Android) para escolher os apps-alvo e configurar
IP/porta do proxy, sem precisar editar nada manualmente.

Este é um projeto em desenvolvimento/teste — o README completo (com tutorial de instalação,
capturas de tela etc.) será escrito após a validação em dispositivo real.

## Estrutura

- `module/` — módulo nativo (C++/Zygisk)
- `manager-app/` — app gerenciador (Kotlin/Jetpack Compose)
- `scripts/build_module_zip.sh` — empacota o `.zip` flashável
- `docs/` — documentação técnica e relatório de desenvolvimento

Veja [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) e [`docs/BUILD.md`](docs/BUILD.md) para detalhes.

## Aviso

Uso destinado à análise de tráfego autorizada (pentest mobile, engenharia reversa, pesquisa de
segurança) em aplicativos que você tem permissão para testar.

---
Desenvolvido por Eduardo Lopes
