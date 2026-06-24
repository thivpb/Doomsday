#include "../../include/telas.h"
#include "../../include/asset_manager.h"
#include <stdio.h>
#include <string.h>

// Banco de perguntas do Quiz da Vigilância Epidemiológica / SUS (DF e Brasil).
// Cada pergunta tem 4 alternativas plausíveis, a correta e uma explicação curta.
typedef struct QuizQuestion {
    const char *question;
    const char *options[4];
    int correctOption;
    const char *explanation;
} QuizQuestion;

static QuizQuestion quizDB[] = {
    // ---- Princípios e diretrizes do SUS ----
    { "Qual destes e um PRINCIPIO doutrinario do SUS?",
      {"Regionalizacao", "Universalidade", "Hierarquizacao", "Descentralizacao"}, 1,
      "Universalidade e principio doutrinario; os outros sao diretrizes organizativas." },
    { "O principio da INTEGRALIDADE no SUS significa:",
      {"Atender so a queixa do momento", "Articular prevencao, tratamento e reabilitacao", "Cobrar conforme a complexidade", "Restringir o cuidado as urgencias"}, 1,
      "Integralidade = acoes de promocao, prevencao e assistencia de forma articulada." },
    { "O principio da EQUIDADE no SUS busca:",
      {"Oferecer servicos identicos a todos", "Investir mais em quem mais precisa", "Atender conforme a renda do usuario", "Concentrar recursos nas capitais"}, 1,
      "Equidade trata os desiguais de forma desigual para alcancar justica em saude." },
    { "Sao DIRETRIZES organizativas do SUS:",
      {"Universalidade e integralidade", "Descentralizacao e participacao social", "Gratuidade e equidade", "Centralizacao e hierarquia"}, 1,
      "Diretrizes organizam o sistema; universalidade/equidade sao principios doutrinarios." },
    { "A DESCENTRALIZACAO do SUS significa principalmente:",
      {"Concentrar as decisoes na Uniao", "Direcao unica em cada esfera de governo", "Extinguir a gestao municipal", "Transferir tudo a rede privada"}, 1,
      "Cada esfera (Uniao, estado, municipio) tem direcao unica e responsabilidades proprias." },
    { "A HIERARQUIZACAO/REGIONALIZACAO organiza a rede em:",
      {"Niveis de complexidade por regiao", "Filas unicas de alcance nacional", "Unidades sem referencia entre si", "Apenas servicos de alta complexidade"}, 0,
      "Atencao primaria, media e alta complexidade conectadas em redes regionais." },
    { "O CONTROLE SOCIAL no SUS e exercido principalmente por meio de:",
      {"Conselhos e Conferencias de Saude", "Ouvidorias do poder legislativo", "Sindicatos de categorias profissionais", "Operadoras de planos de saude"}, 0,
      "Conselhos de Saude sao paritarios e deliberativos (Lei 8.142/90)." },
    { "Nos Conselhos de Saude, os USUARIOS ocupam quantos por cento das vagas?",
      {"25% das vagas", "50% das vagas (paridade)", "10% das vagas", "75% das vagas"}, 1,
      "A representacao dos usuarios e paritaria: 50% do total de conselheiros." },

    // ---- Vigilancia, notificacao, epidemiologia ----
    { "A VIGILANCIA EPIDEMIOLOGICA tem como funcao:",
      {"Inspecionar alimentos e produtos", "Detectar e prevenir mudancas nas doencas", "Distribuir medicamentos na rede", "Habilitar leitos hospitalares"}, 1,
      "Ela monitora doencas e agravos para orientar acoes de controle." },
    { "NOTIFICACAO COMPULSORIA e:",
      {"Comunicacao opcional ao gestor", "Comunicacao obrigatoria de doencas as autoridades", "Registro voluntario em prontuario", "Aviso interno entre setores"}, 1,
      "Certas doencas DEVEM ser notificadas para vigilancia e resposta rapida." },
    { "Qual sistema registra as notificacoes de doencas no Brasil?",
      {"SINAN", "SISREG", "SIASG", "SIAFI"}, 0,
      "SINAN = Sistema de Informacao de Agravos de Notificacao." },
    { "Qual a diferenca entre ENDEMIA e EPIDEMIA?",
      {"Sao termos equivalentes", "Endemia e o nivel habitual; epidemia, a alta incomum", "Endemia atinge varios continentes", "Epidemia sempre dura mais que endemia"}, 1,
      "Endemia = base esperada numa regiao; epidemia = elevacao incomum de casos." },
    { "Uma PANDEMIA caracteriza-se por:",
      {"Casos restritos a um bairro", "Epidemia espalhada por varios paises", "Doenca de ocorrencia rara", "Agravo exclusivo de animais"}, 1,
      "Pandemia e a disseminacao mundial de uma doenca, como a COVID-19." },
    { "Um SURTO e:",
      {"Aumento subito de casos em local especifico", "Doenca de evolucao cronica", "Campanha de imunizacao", "Reacao adversa a vacina"}, 0,
      "Surto = elevacao localizada de casos (ex.: numa escola ou bairro)." },
    { "VIGILANCIA SANITARIA atua principalmente em:",
      {"Notificacao compulsoria de doencas", "Risco sanitario de produtos e servicos", "Visitas domiciliares as familias", "Controle ambiental de vetores"}, 1,
      "Fiscaliza alimentos, medicamentos, servicos de saude e saneantes." },
    { "VIGILANCIA AMBIENTAL em saude monitora:",
      {"Fiscalizacao de medicamentos", "Fatores do ambiente: agua, ar e vetores", "Cobertura vacinal da populacao", "Notificacao de agravos agudos"}, 1,
      "Inclui qualidade da agua, contaminantes e controle de vetores e zoonoses." },

    // ---- Atencao primaria / ESF / RAS ----
    { "A ATENCAO PRIMARIA a Saude (APS) e a porta de entrada e atua na:",
      {"Realizacao exclusiva de cirurgias", "Promocao, prevencao e cuidado continuo", "Atencao apenas de alta complexidade", "Internacao em leitos de UTI"}, 1,
      "A APS coordena o cuidado e resolve a maioria dos problemas de saude." },
    { "A Estrategia Saude da Familia (ESF) tem como base a equipe com:",
      {"Apenas medicos especialistas", "Medico, enfermeiro, tecnicos e ACS", "Somente agentes administrativos", "Exclusivamente cirurgioes-dentistas"}, 1,
      "A ESF organiza a APS com equipe multiprofissional e territorio definido." },
    { "O Agente Comunitario de Saude (ACS) atua principalmente:",
      {"No centro cirurgico do hospital", "Visitando familias e ligando-as a UBS", "No laboratorio de analises", "Na farmacia de alto custo"}, 1,
      "O ACS conhece o territorio e aproxima a comunidade dos servicos." },
    { "A REDE DE ATENCAO A SAUDE (RAS) busca:",
      {"Manter os servicos isolados", "Integrar os niveis de cuidado de forma coordenada", "Priorizar apenas os hospitais", "Substituir a atencao primaria"}, 1,
      "RAS conecta APS, especializada e hospitalar com continuidade do cuidado." },
    { "O que e a referencia e contrarreferencia?",
      {"Mudar de equipe responsavel", "Encaminhar ao especialista e retornar a APS", "Cancelar uma consulta agendada", "Cobrar pelo exame realizado"}, 1,
      "Garante a continuidade do cuidado entre os niveis de atencao." },

    // ---- Vacinacao ----
    { "A vacinacao em massa protege a comunidade pelo efeito de:",
      {"Imunidade de rebanho (coletiva)", "Resposta do tipo placebo", "Reacao alergica em cadeia", "Tolerancia imunologica adquirida"}, 0,
      "Quando muitos estao imunes, a circulacao do agente diminui e protege todos." },
    { "O Programa Nacional de Imunizacoes (PNI) e responsavel por:",
      {"Comercializar vacinas importadas", "Ofertar vacinas gratuitas pelo SUS", "Regular o preco dos medicamentos", "Fiscalizar as clinicas privadas"}, 1,
      "O PNI e referencia mundial em vacinacao publica e gratuita." },
    { "A vacina da Gripe (Influenza) deve ser tomada:",
      {"Apenas uma vez na vida", "Todo ano, pois o virus sofre mutacoes", "A cada dez anos", "Somente apos os 60 anos"}, 1,
      "A composicao muda a cada ano conforme as cepas circulantes." },
    { "Caderneta de vacinacao em dia serve para:",
      {"Servir de documento de identidade", "Registrar e acompanhar as doses recebidas", "Garantir desconto em farmacias", "Substituir a receita medica"}, 1,
      "Ajuda equipe e usuario a controlar o calendario vacinal." },

    // ---- Dengue / Aedes / arboviroses ----
    { "Qual o principal vetor da Dengue, Zika e Chikungunya?",
      {"Mosquito Anopheles", "Aedes aegypti", "Mosquito-palha (flebotomineo)", "Barbeiro (triatomineo)"}, 1,
      "O Aedes aegypti transmite essas arboviroses urbanas." },
    { "A melhor forma de evitar criadouros do Aedes e:",
      {"Eliminar ou cobrir a agua parada", "Manter pratos de plantas com agua", "Armazenar pneus ao ar livre", "Deixar a caixa-d'agua aberta"}, 0,
      "Sem agua parada, o mosquito nao se reproduz." },
    { "Sinais de alarme da Dengue grave incluem:",
      {"Coriza e dor de garganta", "Dor abdominal intensa e sangramentos", "Coceira leve na pele", "Cansaco apos esforco fisico"}, 1,
      "Dor abdominal, vomitos persistentes e sangramento exigem atendimento imediato." },
    { "Onde descartar pneus velhos para evitar a Dengue?",
      {"Em terrenos baldios", "Em ecopontos ou locais cobertos", "As margens de rios", "No quintal a ceu aberto"}, 1,
      "Pneus acumulam agua e viram criadouro; descarte correto evita focos." },
    { "A Doenca de Chagas e transmitida classicamente pelo:",
      {"Aedes aegypti", "Barbeiro (triatomineo)", "Caramujo de agua doce", "Carrapato-estrela"}, 1,
      "O 'barbeiro' transmite o Trypanosoma cruzi." },

    // ---- Atribuicoes das esferas / financiamento ----
    { "Cabe principalmente ao MUNICIPIO no SUS:",
      {"Definir as normas nacionais de saude", "Executar e gerir os servicos locais", "Coordenar o SUS em todo o pais", "Cofinanciar com recursos federais"}, 1,
      "O municipio e o principal executor das acoes de saude (APS)." },
    { "Cabe a UNIAO no SUS, sobretudo:",
      {"Coordenacao nacional e normas gerais", "Visita domiciliar as familias", "Gestao da unidade basica local", "Execucao direta das consultas"}, 0,
      "A Uniao formula politicas nacionais e participa do financiamento." },
    { "O financiamento do SUS e responsabilidade:",
      {"Apenas do governo federal", "Das tres esferas: Uniao, estados e municipios", "Somente dos municipios", "Sobretudo da rede privada"}, 1,
      "E um financiamento tripartite, definido em lei." },
    { "A RENAME e:",
      {"A lista de hospitais credenciados", "A relacao de medicamentos essenciais", "Um tributo destinado a saude", "O cadastro nacional de medicos"}, 1,
      "Orienta a oferta de medicamentos essenciais na rede publica." },
    { "A RENASES e:",
      {"A relacao de acoes e servicos do SUS", "Um medicamento de alto custo", "Um conselho gestor de saude", "Um exame laboratorial padrao"}, 0,
      "Define o rol de acoes e servicos oferecidos pelo SUS." },
    { "O Cartao Nacional de Saude (CNS) serve para:",
      {"Identificar o usuario e seus atendimentos", "Comprovar o vinculo empregaticio", "Substituir a caderneta de vacina", "Autorizar a compra de remedios"}, 0,
      "Vincula o cidadao a seus registros e facilita a continuidade do cuidado." },

    // ---- Promocao/prevencao e determinantes ----
    { "DETERMINANTES SOCIAIS da saude sao:",
      {"Apenas fatores geneticos", "Moradia, renda, educacao e saneamento", "Somente agentes infecciosos", "Exclusivamente a idade da pessoa"}, 1,
      "A saude depende fortemente das condicoes de vida e trabalho." },
    { "PROMOCAO da saude difere de PREVENCAO porque:",
      {"Sao conceitos equivalentes", "Promocao age nas condicoes de vida; prevencao, no risco", "Promocao ocorre so no hospital", "Prevencao depende so de remedios"}, 1,
      "Promocao atua sobre determinantes; prevencao reduz risco de agravos especificos." },
    { "Saneamento basico impacta a saude porque:",
      {"Tem efeito apenas estetico", "Agua tratada e esgoto reduzem infeccoes", "Beneficia somente a economia", "Afeta so o conforto das casas"}, 1,
      "Saneamento e uma das maiores medidas de saude publica da historia." },
    { "Por que NAO usar antibiotico sem prescricao?",
      {"Porque costuma ser caro", "Porque seleciona bacterias resistentes (KPC)", "Porque tem efeito muito lento", "Porque exige jejum prolongado"}, 1,
      "O uso indevido seleciona superbacterias resistentes — um risco global." },
    { "Medida eficaz para frear epidemia respiratoria:",
      {"Reduzir a ventilacao dos ambientes", "Higiene das maos, etiqueta e vacinacao", "Compartilhar utensilios pessoais", "Aguardar a imunidade natural"}, 1,
      "Medidas simples de higiene e vacinacao quebram a cadeia de transmissao." },
    { "Zoonoses sao doencas:",
      {"Exclusivas de aves domesticas", "Transmitidas entre animais e humanos", "Hereditarias, passadas dos pais", "Provocadas pela agua tratada"}, 1,
      "Ex.: raiva e leptospirose; a vigilancia ambiental atua no controle." },
    { "A Atencao Primaria deve ser RESOLUTIVA, ou seja:",
      {"Encaminhar todos os casos ao hospital", "Resolver a maioria dos casos no territorio", "Apenas agendar exames de rotina", "Limitar-se a emitir atestados"}, 1,
      "Boa APS resolve ate ~80% das demandas de saude da populacao." },
    { "Educacao em saude na comunidade serve para:",
      {"Tem pouca utilidade pratica", "Capacitar as pessoas a cuidar da saude", "Incentivar o consumo de remedios", "Reduzir o numero de equipes"}, 1,
      "Informacao e participacao melhoram desfechos de saude coletiva." },

    // ---- Microbiologia / conceitos do jogo (expansao 2 Mundos) ----
    { "O que sao BACTERIOFAGOS?",
      {"Antibioticos de origem natural", "Virus que infectam bacterias", "Um tipo de vacina oral", "Celulas do sistema de defesa"}, 1,
      "Bacteriofagos sao virus que infectam bacterias — nao confundir com antibiotico." },
    { "Para que servem as VACINAS?",
      {"Curar qualquer doenca de imediato", "Treinar o sistema imune contra o patogeno", "Dispensar os habitos de higiene", "Eliminar bacterias ja resistentes"}, 1,
      "Vacinas preparam a imunidade contra doencas, sobretudo virais (ex.: influenza)." },
    { "O CAPSIDEO de um virus e:",
      {"O nucleo de material genetico", "A capa proteica que protege o genoma", "Uma classe de antibiotico", "Uma celula de defesa do corpo"}, 1,
      "Neutralizar/romper o capsideo ajuda o sistema imune a conter o virus." },
    { "Dengue e influenza sao causadas por:",
      {"Bacterias", "Virus de RNA", "Fungos", "Protozoarios"}, 1,
      "Sao virus de RNA; a dengue e transmitida pelo Aedes aegypti." }
};

#define QUIZ_COUNT ((int)(sizeof(quizDB) / sizeof(quizDB[0])))
#define HISTORY_SIZE 14   // evita repetir as ~14 perguntas mais recentes na run

static int currentQuestionIdx = 0;
static int history[HISTORY_SIZE];
static int historyCount = 0;
static bool questionAnswered = false;
static int selectedOption = -1;
static int shuffledCorrect = 0;     // posição (0..3) da alternativa correta APÓS embaralhar
static bool quizInitialized = false;

static UIButton quizOptionsBtn[4];

static bool InHistory(int idx)
{
    for (int i = 0; i < historyCount; i++)
        if (history[i] == idx) return true;
    return false;
}

static void PushHistory(int idx)
{
    if (historyCount < HISTORY_SIZE) history[historyCount++] = idx;
    else { for (int i = 1; i < HISTORY_SIZE; i++) history[i - 1] = history[i]; history[HISTORY_SIZE - 1] = idx; }
}

// Monta os botoes da pergunta atual EMBARALHANDO as 4 alternativas (Fisher-Yates),
// para que a correta nao fique sempre na mesma posicao (antes ~64% no indice 1).
// Define shuffledCorrect = posicao (0..3) da alternativa correta apos embaralhar.
static void QuizPrepareOptions(void)
{
    int perm[4] = { 0, 1, 2, 3 };
    for (int i = 3; i > 0; i--) {
        int j = GetRandomValue(0, i);
        int t = perm[i]; perm[i] = perm[j]; perm[j] = t;
    }
    int correctOrig = quizDB[currentQuestionIdx].correctOption;
    shuffledCorrect = 0;
    for (int i = 0; i < 4; i++) {
        quizOptionsBtn[i] = (UIButton){ { 240, 290 + i * 64, 800, 48 }, quizDB[currentQuestionIdx].options[perm[i]], false, false };
        if (perm[i] == correctOrig) shuffledCorrect = i;
    }
}

void DrawTelaQuiz(GameState *game, Font font)
{
    if (!quizInitialized) {
        // Sorteia uma pergunta que NAO esteja no historico recente (baixa repeticao)
        int tries = 0;
        do {
            currentQuestionIdx = GetRandomValue(0, QUIZ_COUNT - 1);
            tries++;
        } while (InHistory(currentQuestionIdx) && tries < 200 && historyCount < QUIZ_COUNT);
        PushHistory(currentQuestionIdx);
        questionAnswered = false;
        selectedOption = -1;
        QuizPrepareOptions();   // monta+embaralha as 4 alternativas desta pergunta
        quizInitialized = true;
    }

    DrawMenuFXBackground((float)GetTime(), game->screenAnim / 0.35f);
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade((Color){ 4, 6, 16, 255 }, 0.42f));

    const char *title = "QUIZ DA VIGILANCIA EPIDEMIOLOGICA / SUS";
    Vector2 titleSz = MeasureTextEx(font, title, 30.0f, 1.0f);
    DrawTextEx(font, title, (Vector2){ (SCREEN_WIDTH / 2.0f) - (titleSz.x / 2.0f), 70.0f }, 30.0f, 1.0f, THEME_COLOR_MAIN);

    const char *question = quizDB[currentQuestionIdx].question;
    Vector2 qSz = MeasureTextEx(font, question, 24.0f, 1.0f);
    DrawTextEx(font, question, (Vector2){ (SCREEN_WIDTH / 2.0f) - (qSz.x / 2.0f), 175.0f }, 24.0f, 1.0f, WHITE);

    for (int i = 0; i < 4; i++) {
        Color baseCol = THEME_COLOR_PANEL;
        Color borderCol = THEME_COLOR_BORDER;
        Color textCol = WHITE;

        if (questionAnswered) {
            if (i == shuffledCorrect) {
                baseCol = Fade(GREEN, 0.4f); borderCol = GREEN; textCol = GREEN;
            } else if (i == selectedOption) {
                baseCol = Fade(RED, 0.4f); borderCol = RED; textCol = RED;
            }
        } else if (quizOptionsBtn[i].hover) {
            baseCol = Fade(THEME_COLOR_BORDER, 0.8f); borderCol = THEME_COLOR_MAIN; textCol = THEME_COLOR_MAIN;
        }

        DrawRectangleRounded(quizOptionsBtn[i].bounds, 0.2f, 6, baseCol);
        DrawRectangleRoundedLines(quizOptionsBtn[i].bounds, 0.2f, 6, borderCol);
        DrawTextEx(font, TextFormat("%c) %s", 'A' + i, quizOptionsBtn[i].text),
                   (Vector2){ quizOptionsBtn[i].bounds.x + 20, quizOptionsBtn[i].bounds.y + 14 }, 20.0f, 1.0f, textCol);
    }

    if (questionAnswered) {
        bool correct = (selectedOption == shuffledCorrect);
        const char *msg = correct ? "CORRETO! +50 PONTOS DO SUS" : "INCORRETO!";
        Color msgCol = correct ? GREEN : RED;
        Vector2 msgSz = MeasureTextEx(font, msg, 26.0f, 1.0f);
        DrawTextEx(font, msg, (Vector2){ (SCREEN_WIDTH / 2.0f) - (msgSz.x / 2.0f), 560.0f }, 26.0f, 1.0f, msgCol);

        // Explicacao curta da resposta correta (educativo)
        const char *exp = quizDB[currentQuestionIdx].explanation;
        Vector2 eSz = MeasureTextEx(font, exp, 17.0f, 1.0f);
        DrawTextEx(font, exp, (Vector2){ (SCREEN_WIDTH / 2.0f) - (eSz.x / 2.0f), 598.0f }, 17.0f, 1.0f, Fade(WHITE, 0.85f));

        const char *cont = "Clique em qualquer lugar para continuar...";
        Vector2 contSz = MeasureTextEx(font, cont, 16.0f, 1.0f);
        DrawTextEx(font, cont, (Vector2){ (SCREEN_WIDTH / 2.0f) - (contSz.x / 2.0f), 650.0f }, 16.0f, 1.0f, GRAY);
    }
}

void UpdateTelaQuiz(GameState *game, Vector2 mouse)
{
    if (questionAnswered) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            quizInitialized = false;
            game->currentScreen = SCREEN_UPGRADE; // segue para a tela de upgrades
        }
        return;
    }

    for (int i = 0; i < 4; i++) {
        bool wasHovered = quizOptionsBtn[i].hover;
        bool isHovered = CheckCollisionPointRec(mouse, quizOptionsBtn[i].bounds);
        quizOptionsBtn[i].hover = isHovered;
        if (isHovered != wasHovered && g_assets.sfxQuizHover.frameCount > 0)
            PlaySound(g_assets.sfxQuizHover);
        if (isHovered) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (g_assets.sfxMenuClick.frameCount > 0) PlaySound(g_assets.sfxMenuClick);
                selectedOption = i;
                questionAnswered = true;
                if (selectedOption == shuffledCorrect) {
                    game->player.susPoints += 50;
                }
            }
        }
    }
}

// Apenas para ferramentas de preview offline (tools/ui_preview.c): força uma
// pergunta especifica e, opcionalmente, um estado ja respondido (selectedShown =
// posicao escolhida; -1 = nao respondida). NAO e chamado no jogo.
void QuizPreviewForce(int qIdx, int selectedShown)
{
    currentQuestionIdx = (qIdx < 0 ? 0 : qIdx) % QUIZ_COUNT;
    QuizPrepareOptions();
    if (selectedShown == -2) selectedShown = (shuffledCorrect + 1) % 4; // força uma errada
    selectedOption = selectedShown;
    questionAnswered = (selectedShown >= 0);
    quizInitialized = true;
}
