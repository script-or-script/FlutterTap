# FlutterTap — Prompts originais da sessão de desenvolvimento

Registro de todos os pedidos/instruções que Eduardo Lopes deu ao assistente durante o
desenvolvimento deste projeto (do zero até a pausa em 2026-07-27), na ordem em que
foram dados. Guardado para reuso como modelo/roteiro em um projeto futuro semelhante
(ex: portar outro script Frida para um módulo Magisk/KernelSU/APatch).

Nota: este arquivo é uma referência pessoal de processo, não documentação do produto.
Avalie se faz sentido incluí-lo no repositório público do GitHub ou mantê-lo só localmente.

---

## 1. Pedido inicial (interpretar o script e criar o módulo)

> é possível voce interpretar um script frida, e criar e desenvolver um aplicativo android
> para Magisk/Apatch/KernelSu baseado no script? Caso precise configurar algum mcp para
> android studio ou algo, voce pode

## 2. Respostas às perguntas de escopo + lista grande de requisitos extras

> 1. O script Frida é esse "C:\Users\eduardo.lopes\Estudos\flutter+burp.js", preciso que o
> módulo faça exatamente a mesma coisa que o script e que seja alteravel e gerenciavel o
> endereço de ip e porta igual no script.
> 2. Não existe um contexto do app alvo, precise que isso seja flexivel e gerenciavel igual
> a maioria do módulos Magisk /KernelSu, de selecionar na hora quais apps vao ser hookados.
> 3. Não, não tenho nada criado e não faço ideia de como fazer ou começár, preciso que façá
> tudo, caso precise configurar coisas com Android Studio na minha máquina, voce pode.
> 4. Ok, pode usar oq voce quiser e preferir, pode configurar tudo que quiser. Se precisar
> acessar documentações em site, baixar coisas na minha máquina ou executar, voce pode,
> qualquer coisa está autorizado a fazer.
>
> Só pra deixar claro, preciso que módulo funcione em versões do Android 10 até o Android 17...
>
> Observação: Vou precisar me ausentar aqui agora e daqui uma hora eu volto, pode ir
> trabalhando nisso sem eu por enquanto por favor
>
> Esqueci de avisar, adicione dentro do gerenciador UI a opção de idioma (Portugues BR,
> Ingles e Chines), coloque um identificador inteligente do tema do sistema (por exemplo eu
> estava usando tema dark no android mas a tela de fundo do gerenciador estava branco ai
> fica ruim).
>
> Remova do texto do titulo do módulo a parte que voce menciona o script, nao precisa falar
> sobre ele da descrição do módulo (destaquei nessa figura "" a parte que estou falando).
>
> Aproveita e desenvolva a parte x86_64 para suportar em emuladores também por favor.
>
> Tenta também colocar uma assinatura minha em algum lugar caracterisco de assinaturas de
> devs no app no código e na UI pois eu irei postar o projeto no github. Coloca tipo "by
> Eduardo Lopes". Outra coisa também, vai documentando tudo que faz em um relatório pdf, ou
> documenta tudo na hora em que acabar, porcessos de como foi desenvolvido e etc.
> Posteriomente depois de tudo isso e eu testar se deu certo, vou precisar que me ajude a
> postar no Github, tutorial e informaçoes caracteristicas iguais aos projetos de outros
> módulos que ja existe na internet preciso ir dormir, voce consegue continuar trabalhando
> aqui sem eu até amanha de manha?

## 3. Retomada — pedindo orientação de teste

> preciso fazer oq agora entao pra começar a testar, nao entendi

## 4. Pedido de documentação dos arquivos de instalação

> quero entender quais sao os arquivos de instalação e passo a passo para eu instalar
> manualmente em outro aparelho do zero, até porque eu irei postar o projeto no github

## 5. Pedido de PDF + preservar contexto da sessão

> ok, gera um pdf disso e guarda todo esse terminal para que possa te passar ele amanha ou
> depois pra voce me ajudar a escrever e publicar o módulo no github e na página oficial
> magisk modules

## 6. Pergunta de estimativa de tempo

> se voce for subir esse projeto no github pra mim, quanto tempo voce estima? demora?

## 7. Autorização pra opção A (gh auth) + pedido de app de treinamento alternativo

> ok vou fazer a opção A. Mas procure na internet um aplicativo flutter de treinamento
> pentenst em Flutter para que possamos usar de exemplo, pois não quero utilizar esses que
> tenho pois são de clientes e não poderão sair nas figuras, e ai vamos instalar um de
> treinamento no aparelho e usar como cobaia para as evidencias

## 8. Confirmação de limpeza do histórico do Burp (dado sensível encontrado)

> eu mesmo limpei, pode prosseguir sem problmas

## 9. Reforço do requisito de generalidade + relato de teste próprio (Nike) + sugestão OWASP MASTG

> Uma observação que preciso deixar clara é que, esse módulo precisa ser inteligente assim
> como o script frida que te aprensentei, ele precisa bypass ssl flutter e servir para todos
> ou grande maioria pelo menos dos apps do mundo inteiro, ou seja flexível não apenas para
> este app de teste que voce está testando agora, esse é só para exemplo do post no github.
> 1 - Sim, FlutterBurpUnpin é minha e está desabilitada por enquanto. Confirmei aqui o seu
> módulo e está funcionando para um aplicativo de meu cliente o Nike, porém percebi que está
> com um bug que só começa a interceptar se eu abrir o app gerenciador mais uma vez depois
> de ter selecionado o alvo la dentro, ai nao sei se é porque ele passa a consumir
> privilégio root do magisk e começa ser efetivo só nessa segunda vez que abre ou se é algum
> delay, nao sei explicar. Caso voce persista muito com erros pra bypass do ssl flutter
> desse app cobaia que estamos usando para teste, pode usar outro bem nomeado na internet
> sobre flutter para teste de pentest, será que esses https://mas.owasp.org/crackmes/ do MAS
> da Owasp não teria flutter pra esse teste?

## 10. Autorização cautelosa para continuar investigando (com receio de regressão)

> Continue investigando esse registrador do GetSockAddr, porém tenho medo de voce tentar
> arrumar e quebrar o módulo que já está funcionando, talvez o problema seja esse DVFA de
> teste, não achou outro app de teste alternativo na internet mesmo? Caso queira ir rodando
> o módulo apontado pro app Nike pra voce poder ir comparando as diferenças e motivos, voce
> pode

## 11. Autorização pra usar o Nike + dois apps alternativos encontrados pelo usuário

> Voce pode sim usar o Nike para comparação ai, a conta logada é minha e eu do total
> permissão. Eu achei mais dois apps alternativos
> "https://github.com/Ostorlab/ostorlab_insecure_android_app" e
> "https://github.com/anasachoury/VulnApp", pode usar um deles, de sua preferencia ou os
> dois caso consiga como cobaia agora, pode descartar esse que voce estava usando esse tal
> de DVFA ai.

## 12. Pedido para repetir o processo no segundo app

> ok agora que deu certo, faça nesse também "https://github.com/anasachoury/VulnApp"

## 13. Pergunta técnica sobre o comportamento observado

> Porque q nesse VulnApp, não passou nada no momento do login, não há requisição no momento
> em que enviamos as credenciais em login?

## 14. Pedido final desta sessão: atualizar PDF + salvar memória + salvar este arquivo

> ok, gera ou atualize o relatório em pdf descrevendo tutorial e passo a passo, coloca
> também explicações sobre o desenvolvimento e etc, não se esqueça de remover o reqable como
> exemplo, coloque apenas os apps de treinamento que fizemos agora a pouco e que deu certo,
> amanha se for o caso a gente popula com figuras de exemplos. Grave tudo em memória dessa
> sessão e conversa porque terei q desligar a máquina e amanha eu continuo exatamente de
> onde paramos. Salve pra mim pfv todos os inputs e prompts q te dei pra q um dia eu possa
> usar os mesmos pedidos para outro projeto

---

## Roteiro resumido (para reaplicar em outro projeto)

Se for repetir esse processo para outro script/ferramenta:

1. Apresente o script/ferramenta original e peça a portagem para a plataforma alvo,
   deixando claro se precisa ser **genérico** (funcionar para qualquer alvo) ou específico.
2. Defina requisitos de UI/gerenciamento logo de cara (idiomas, tema, configuração em
   runtime) — economiza retrabalho vs. adicionar depois.
3. Autorize explicitamente autonomia (baixar dependências, instalar SDKs, mexer no
   dispositivo) se quiser que o trabalho avance sem pausas para confirmação a cada passo.
4. Peça documentação contínua (relatório) desde o início — mais barato que reconstruir a
   história depois.
5. Na hora de validar: se tiver dispositivo real rooteado à mão, deixe explícito que pode
   ser usado — evita ida e volta de "como eu testo isso".
6. Para evidências públicas (screenshots, README): avise cedo que apps de cliente/pessoais
   não podem aparecer, e peça para buscar apps de treinamento/pentest públicos e conhecidos
   como substitutos.
7. Quando pedir para investigar um bug: deixe claro o quanto de risco de regressão é
   aceitável ("pode arriscar mudar a lógica principal" vs. "não quero perder o que já
   funciona") — isso muda a estratégia de correção.
8. Ao encerrar uma sessão longa: peça explicitamente para (a) atualizar a documentação/PDF,
   (b) salvar o estado em memória persistente do assistente, e (c) registrar os prompts
   usados, se quiser reaplicar o mesmo roteiro depois.
