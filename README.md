# FlutterTap

Módulo Zygisk (Magisk / KernelSU / SukiSu Ultra / APatch) que redireciona o tráfego de rede de
apps Flutter selecionados para um proxy configurável e contorna a verificação de certificado TLS,
para fins de análise de tráfego. Inclui um app gerenciador (Android) para escolher os apps-alvo e
configurar IP/porta do proxy, sem precisar editar nada manualmente.

Validado em dispositivo real capturando tráfego de ponta a ponta, em Android 10 (Magisk +
NeoZygisk) e Android 17 (SukiSu Ultra + Zygisk Next, inclusive com o **Zygisk Next Linker**
ativo). Capturas de tela em [`docs/screenshots/`](docs/screenshots/).

O README completo (tutorial de instalação passo a passo, capturas comentadas) ainda será escrito.

## Estrutura

- `module/` — módulo nativo (C++/Zygisk)
- `manager-app/` — app gerenciador (Kotlin/Jetpack Compose)
- `scripts/build_module_zip.sh` — empacota o `.zip` flashável
- `docs/` — documentação técnica e relatório de desenvolvimento

Veja [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) e [`docs/BUILD.md`](docs/BUILD.md) para detalhes.

## Aviso

Uso destinado à análise de tráfego autorizada (pentest mobile, engenharia reversa, pesquisa de
segurança) em aplicativos que você tem permissão para testar.

## Licença

MIT — veja [`LICENSE`](LICENSE). O módulo nativo linka estaticamente Dobby (Apache-2.0) e
Capstone (BSD-3); os componentes de terceiros e suas licenças estão listados em
[`THIRD_PARTY.md`](THIRD_PARTY.md), e os textos completos acompanham o `.zip` distribuído.

---
Desenvolvido por Eduardo Lopes
