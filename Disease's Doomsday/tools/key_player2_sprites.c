// key_player2_sprites.c
// Utilitário OFFLINE (mesmo padrão de tools/bake_collision_mask.c): recebe um PNG
// com fundo PRETO sólido (vindo de um JPEG decodificado por `sips`) e exporta um
// PNG com o fundo preto chaveado para TRANSPARENTE. Usado para preparar as folhas
// de sprite do personagem "ANTICORPO-V" (fundo preto -> alpha) sem depender de
// Pillow/ImageMagick, que não estão instalados.
//
// Estratégia (com FEATHER p/ matar o "halo" cinza do JPEG ao redor do contorno):
//   m = max(r,g,b)
//     m <= THR_LOW            -> alpha 0   (fundo preto puro)
//     THR_LOW < m < THR_HIGH  -> alpha proporcional (borda suavizada)
//     m >= THR_HIGH           -> alpha 255 (corpo do personagem, inclusive partes
//                                           navy/teal escuras, que NÃO são preto puro)
// THR_LOW baixo preserva as cores escuras do corpo; calibrável por argv.
//
// LoadImage/ExportImage operam na CPU (stb) e NÃO exigem janela/contexto GL — roda
// totalmente headless.
//
// Uso:
//   key_player2_sprites <in.png> <out.png> [THR_LOW=16] [THR_HIGH=48]

#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("uso: %s <in.png> <out.png> [THR_LOW=16] [THR_HIGH=48]\n", argv[0]);
        return 2;
    }
    const char *inPath  = argv[1];
    const char *outPath = argv[2];
    int thrLow  = (argc > 3) ? atoi(argv[3]) : 16;
    int thrHigh = (argc > 4) ? atoi(argv[4]) : 48;
    if (thrHigh <= thrLow) thrHigh = thrLow + 1;

    SetTraceLogLevel(LOG_WARNING);

    if (!FileExists(inPath))
    {
        printf("[ERRO] entrada inexistente: %s\n", inPath);
        return 1;
    }

    Image img = LoadImage(inPath);
    if (img.data == NULL || img.width <= 0 || img.height <= 0)
    {
        printf("[ERRO] falha ao carregar imagem: %s\n", inPath);
        return 1;
    }

    // Trabalha sempre em RGBA 32-bit para garantir canal alfa.
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Color *px = LoadImageColors(img);
    int n = img.width * img.height;
    int keyed = 0, feather = 0;

    for (int i = 0; i < n; i++)
    {
        int r = px[i].r, g = px[i].g, b = px[i].b;
        int m = r; if (g > m) m = g; if (b > m) m = b;
        if (m <= thrLow)
        {
            px[i].a = 0;
            keyed++;
        }
        else if (m < thrHigh)
        {
            px[i].a = (unsigned char)((m - thrLow) * 255 / (thrHigh - thrLow));
            feather++;
        }
        // else: mantém alpha 255 (corpo opaco)
    }

    // Reconstrói a imagem a partir dos pixels modificados e exporta.
    Image out = {
        .data = px,
        .width = img.width,
        .height = img.height,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    bool ok = ExportImage(out, outPath);

    UnloadImageColors(px);
    UnloadImage(img);

    if (!ok)
    {
        printf("[ERRO] falha ao exportar: %s\n", outPath);
        return 1;
    }
    printf("[OK] %s -> %s  (%dx%d; preto=%d feather=%d; THR=%d..%d)\n",
           inPath, outPath, out.width, out.height, keyed, feather, thrLow, thrHigh);
    return 0;
}
