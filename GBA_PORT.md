# Porte nativo para Game Boy Advance

Uma ROM nativa já é produzida em `gba/build/ellis_house_gba.gba`. Ela não usa
raylib, OpenGL, sistema de arquivos, heap ou ponto flutuante em runtime.

## Implementado

- Backend bare-metal ARM7TDMI, cabeçalho Nintendo e linker próprios.
- Loop principal, física, colisões, inimigos, áudio e rotinas críticas de desenho
  executam na IWRAM; acesso à ROM usa prefetch e espera otimizada do cartucho.
- Seis salas jogáveis (`rm_0` a `rm_4` e `rm_boss`) convertidas do manifesto;
  a garagem foi retirada da progressão.
- Posições iniciais e retângulos de saída extraídos das salas originais.
- Cenários/tiles pré-compostos, paleta RGB555 por sala e viewport 240x160.
- Pixels dos sprites preservados em 1:1; não há resize em runtime.
- Física 24.8 em 45 Hz sobre apresentação de 60 Hz, com latch de entrada,
  jump buffer original de 4 ticks e coyote time original de 6 ticks.
- Personagem, câmera e inimigos são interpolados entre os passos de física nos
  60 quadros de apresentação, suavizando o movimento sem acelerar o jogo.
- A câmera aceita colunas pares e ímpares. Uma disposição pré-deslocada dos
  cenários na ROM mantém ambas por DMA e elimina a vibração da personagem sem
  transferir o custo da rolagem para a CPU.
- Wall slide completo: detecção do lado pressionado, queda dividida por 1,75,
  animação `spr_player_walljump` e impulso lateral travado por 10 ticks.
- Colisão sólida e plataformas em máscaras de 1 bit, incluindo rampas e flips.
- O ajuste vertical de até `abs(hsp)` pixels do `scr_move` foi restaurado, então
  rampas podem ser subidas sem transformar paredes comuns em superfícies escaláveis.
- Ao correr descendo rampas/escadas, o apoio inclinado próximo mantém a animação
  de corrida contínua; a animação de queda continua sendo usada em quedas reais.
- Idle, corrida, pulo, queda, wall jump, agachar, slide, dash, blink e morte
  usam todos os frames correspondentes da personagem como OBJ 4bpp.
- Dash e slide criam rastros com os frames de `spr_trail` e `spr_trailslide`,
  composição semitransparente por hardware e duração distinta como no original.
  O quadro do rastro só é enviado novamente à VRAM quando muda, evitando tráfego
  redundante durante slide perto de inimigos e móveis grandes.
- Chocolates e os upgrades `obj_sock`, `obj_lollipop` e `obj_pocket` são
  entidades coletáveis; liberam slide, dash e blink na mesma ordem do original.
- Slide preserva a inércia e reduz `hsp` em 0,05 por tick, conforme `scr_slide`.
  Sua máscara usa a caixa original reduzida (`x=3..17`, `y=10..25`) e só volta
  à altura normal quando há espaço livre acima da personagem.
- HUD usa `spr_chocolat`, `spr_cavera` e dígitos da `font0` em escala 0,75.
- Máscara de perigos estáticos, queda para fora da sala, animação completa de
  morte e reinício automático da sala.
- Os 102 inimigos colocados nas seis fases deixaram de fazer parte do bitmap do
  cenário. Agora são entidades animadas em pool fixo, desenhadas em pixels 1:1,
  com perseguição, queda, investida, arremesso e geradores extraídos do manifesto.
- Ao entrarem no estado ativo, inimigos selecionam continuamente o segundo frame
  com outline, conforme os `image_index = 1` dos scripts GML.
- Geradores criam fireballs, louças e ferramentas em runtime; colisões com os
  inimigos respeitam os pixels transparentes dos frames em vez de uma caixa cheia.
- A luta do chefe alterna os sprites originais de idle/olhos/morte, cria uma
  bola de lixo por ciclo e os dois olhos, e usa os dois efeitos vocais. O ataque
  do carro foi removido exclusivamente desta versão para manter a fluidez.
- Os 72 ângulos da bola de lixo são pré-calculados em passos de 5 graus com os
  pixels originais e intervalos opacos por linha; girar não exige transformação
  por pixel no ARM7TDMI.
- Os quadros gigantes do chefe são pré-compostos com o cenário e transmitidos
  por DMA como a própria janela da sala. O desenho por pixel foi eliminado da
  luta, enquanto projéteis, animações e colisões continuam dinâmicos.
- A barra de vida do chefe voltou, acompanhando os dez segmentos internos da
  luta sobre a moldura original. Moldura e segmentos agora são sprites de
  hardware, sem redesenho por pixel. O efeito `spr_blink` também aparece ao
  atravessar perigos ou inimigos enquanto Elli está intangível.
- Todos os seis efeitos exportados pelo projeto (`jump`, `coin`, `hurt`, `blink`
  e os dois do chefe) estão convertidos para PCM. Os dois FIFOs/DMA reproduzem
  até dois efeitos simultâneos; o silêncio inicial dos arquivos é aparado com
  2 ms de preroll para resposta imediata.
- Tela de título GBA simplificada para desempenho e leitura: `spr_title` original
  em 1:1 no centro e somente `Play`, em `font0`, na parte inferior.
- Pausa restaurada no Start, com o `spr_pause` original localizado, arte 1:1,
  navegação por cima/baixo e as ações continuar, reiniciar e voltar ao menu.
- Os seis títulos de sala usam os sprites originais nos dois idiomas e o fade
  de 50 ticks por alpha blending do hardware. São divididos em OBJs 8bpp sem
  redimensionamento, aparecem só na entrada (não após morte) e não consomem CPU
  de desenho. O blend afeta somente esses OBJs, nunca a personagem.
- Placa inativa no cenário, quadro ativo como OBJ e prompt com D-pad Down.
- Placas usam o `spr_popup`, os textos literais e a `font0` originais. O sprite
  fica em escala 1:1, centralizado; somente os 6 pixels que excedem cada lateral
  física do LCD ficam fora do viewport.
- O gerador lê cada literal diretamente de `obj_placa1..6/Create_0.gml`; não há
  cópia adaptada ou tradução paralela no runtime GBA.
- Double buffering Mode 4 e cópia da janela por DMA.
- O cenário permanece mapeado na ROM: por frame, somente as 240x160 posições
  cobertas pela câmera são lidas por DMA. Inimigos e coletáveis são rejeitados
  antes do desenho quando estão fora do viewport.
- Inimigos usam rejeição ampla antes da colisão pixel-perfect, intervalos opacos
  pré-calculados por linha e escrita de dois pixels por acesso à VRAM. Regiões
  transparentes não são mais percorridas nas fases com muitas entidades. Pares
  totalmente opacos são escritos sem leitura prévia da VRAM, reduzindo o custo
  dos eletrodomésticos animados nas partes mais pesadas da cozinha.
- Os móveis, eletrodomésticos, louças e demais inimigos da cozinha também usam
  sprites de hardware 4bpp depois do título da sala. A consulta de cada sprite é
  direta por ID, sem busca linear por entidade e sem composição na framebuffer.
- Os 44 quadros/inimigos repetidos da escadaria usam sprites de hardware 4bpp
  e uma paleta dedicada, eliminando o principal custo de software de `rm_1`.
- Depois quatro variações grandes e quatro pequenas são mantidas na VRAM após o
  título da sala, devolvendo variedade aos quadros sem sacrificar a fluidez.
- Os quadros atacantes usam alcance circular, direção euclidiana e a oscilação
  original de -9 a +9 graus; movimento e câmera também recebem interpolação.
- As músicas foram removidas da ROM; somente efeitos sonoros são inicializados.
- A ROM não possui New Game Plus nem instancia seus cinco itens exclusivos.
- Após o chefe, `spr_end` ou `spr_trueend` é exibido em pixels 1:1; o final
  verdadeiro preserva o deslocamento horizontal original.
- Slide, dash e blink são liberados no contato com seus upgrades; suas animações
  de coleta continuam, mas uma transição de sala não pode mais perder a habilidade.

## Controles

- D-pad: mover; Down: agachar, slide, ler placa ou descer da plataforma.
- A: pular e wall jump.
- B: dash enquanto se move no ar, após coletar o upgrade.
- Start: pausar/continuar; no pause, cima/baixo escolhem e A confirma.
- R: concede slide, dash e blink e avança para a próxima fase (atalho de debug;
  após a última, volta à primeira).
- A versão GBA usa somente os textos em inglês para reduzir ROM e trabalho de troca.
- A ou Start no menu: jogar.

## Build reproduzível

Requer devkitARM, Python com Pillow e FFmpeg para converter as trilhas e efeitos.
Na pasta `gba`:

```powershell
& "C:\devkitPro\msys2\usr\bin\make.exe" -j2
```

O conversor em `gba/tools/generate_assets.py` lê diretamente
`assets/game.manifest`; portanto correções futuras no conversor desktop também
podem ser regeneradas para a ROM.

## Ainda em migração

O núcleo jogável, as fases, os inimigos e os efeitos de gameplay já estão na ROM.
Ainda faltam migrar algumas telas externas às sete fases (splash separado,
cutscene final/créditos), o save e a iluminação dinâmica específica do hardware.
Somente a ROM GBA deixa de carregar músicas. O executável C++ preserva as faixas
do menu, das fases, do chefe e do final como no projeto original.
