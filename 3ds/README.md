# Elli's House 1.2 — Nintendo 3DS

Porte nativo baseado diretamente em `src/game.cpp`. A lógica de salas, física,
colisões, inimigos, chefe, upgrades e finais é compartilhada com a versão C++.

## Telas

- Superior: jogo em 384x218, centralizado 1:1 dentro da tela 400x240.
- Slider 3D: controla a separação estereoscópica do plano do jogo.
- Inferior: fundo original `bg_menu2`, chocolates e mortes.

## Controles

- Circle Pad / D-Pad: mover
- A: pular / confirmar
- B, X ou Y: dash
- Baixo ou L: agachar / slide / descer plataforma
- Start: pausa / voltar
- R: reiniciar a sala

## Build para Homebrew Launcher ou CIA

No PowerShell, a partir desta pasta:

```powershell
./tools/generate_assets.ps1
$env:DEVKITPRO='/opt/devkitpro'
$env:DEVKITARM='/opt/devkitpro/devkitARM'
C:/devkitPro/msys2/usr/bin/make.exe -j4
```

O resultado é `EllisHouse3DS.3dsx`. Para gerar também o CIA, instale
`bannertool`, `makerom` e `3dstool` no `PATH` (ou passe seus caminhos como
variáveis do Make) e execute `make cia`.

O save é gravado em `sdmc:/3ds/EllisHouse3DS/Save.sav`.
