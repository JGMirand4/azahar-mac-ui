# Azahar Home UI (macOS)

Tela inicial inspirada no HOME do 3DS: carrossel, grade e lista, capas automáticas, modo console e suporte a controle.

## Atalhos
| Tecla | Ação |
|---|---|
| Setas | Navegar |
| Enter | Jogar |
| Esc | Início / voltar |
| F | Favoritar |
| Q / E | Trocar filtro (Todos, Favoritos, Jogados) |
| R | Jogo aleatório |
| / | Buscar |

## Controle
D-pad/analógico: navegar · A: jogar · B: voltar · X: favoritar · Y: aleatório · L/R: filtro · L2/R2: modo de visualização · Select: ordenar · L3: modo console · Start: configurações.

## Configurações
Em Configurações → Interface: estilo dos botões do controle (Auto/PlayStation/Xbox), estilo do ícone dos jogos (Normal / cartucho 3DS) e capas automáticas.

## Gerar o DMG
`tools/make_dmg.sh [versão]` no macOS, ou rode o workflow *release-dmg* no GitHub Actions (artefato `Azahar-dmg`; uma tag `v*` publica um Release).
O app não é assinado: na primeira abertura use botão direito → Abrir (Gatekeeper).
