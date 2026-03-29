/*
 * gerenciador de Buffer com políticas LRU, FIFO, CLOCK e MRU
 *
 * compilação: g++ -o buffer_manager buffer_manager.cpp
 * como usar: ./buffer_manager <arquivo_texto> <política: LRU|FIFO|CLOCK|MRU>
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <list>
#include <algorithm>
#include <cstdlib>
#include <ctime>

using namespace std;

// ─── Constantes ────────────────────────────────────────────────────────────────
const int BUFFER_SIZE = 5; // Número máximo de entradas no buffer

// ─── Estrutura de uma entrada do buffer ────────────────────────────────────────
struct Page {
    int    pageId;       // Identificador da página (page#)
    string content;      // Conteúdo da linha de texto
    bool   dirty;        // Variável de atualização (TRUE = foi modificada)
};

// ─── Políticas disponíveis ─────────────────────────────────────────────────────
enum Policy { LRU, FIFO, CLOCK_POL, MRU };

// ═══════════════════════════════════════════════════════════════════════════════
// Classe BufferManager
// ═══════════════════════════════════════════════════════════════════════════════
class BufferManager {
private:
    // --- Estado do buffer ---
    vector<Page> buffer;
    int  cacheHit  = 0;
    int  cacheMiss = 0;
    Policy policy;

    // --- Estruturas auxiliares por política ---
    list<int> lruOrder;  // front = mais recente, back = menos recente (LRU/MRU)
    list<int> fifoOrder; // front = mais antigo (FIFO)

    // CLOCK: ponteiro circular + bit de referência por posição no buffer
    int          clockHand = 0;
    vector<bool> refBit;

    // --- Arquivo de texto ---
    string filename;

    // ── Utilitários internos ──────────────────────────────────────────────────

    int findInBuffer(int key) {
        for (int i = 0; i < (int)buffer.size(); i++)
            if (buffer[i].pageId == key) return i;
        return -1;
    }

    // Lê a linha de número 'key' (1-indexed, ignorando header) do arquivo CSV.
    // Trata aspas externas e desescapa "" → " no interior.
    string readFromFile(int key) {
        ifstream file(filename);
        if (!file.is_open()) {
            fprintf(stderr, "[ERRO] Não foi possível abrir o arquivo: %s\n", filename.c_str());
            return "";
        }
        string line;
        int lineNum = 0;
        while (getline(file, line)) {
            // Remove \r de arquivos com line endings Windows (CRLF)
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            lineNum++;
            if (lineNum == 1) continue; // ignora header
            if (lineNum - 1 == key) {
                // Remove aspas externas, se existirem
                if (line.size() >= 2 && line.front() == '"' && line.back() == '"')
                    line = line.substr(1, line.size() - 2);

                // Desescapa aspas duplas internas ("" → ")
                string result;
                for (size_t i = 0; i < line.size(); i++) {
                    if (line[i] == '"' && i + 1 < line.size() && line[i + 1] == '"')
                        i++; // consome a aspa extra
                    result += line[i];
                }
                return result;
            }
        }
        return ""; // página não encontrada
    }

    bool randomDirty() {
        return (rand() % 2) == 1;
    }

    // ── Lógica de ordem por política ─────────────────────────────────────────

    void recordAccess(int pageId) {
        lruOrder.remove(pageId);
        lruOrder.push_front(pageId);
    }

    void recordInsert(int pageId) {
        fifoOrder.push_back(pageId);
    }

    void removeFromAux(int pageId) {
        lruOrder.remove(pageId);
        fifoOrder.remove(pageId);
    }

    // ── Seleção de vítima por política ────────────────────────────────────────

    int selectVictim() {
        switch (policy) {

        case LRU: {
            int victim = lruOrder.back();
            return findInBuffer(victim);
        }

        case MRU: {
            int victim = lruOrder.front();
            return findInBuffer(victim);
        }

        case FIFO: {
            int victim = fifoOrder.front();
            return findInBuffer(victim);
        }

        case CLOCK_POL: {
            while (true) {
                if (!refBit[clockHand]) {
                    int victimIdx = clockHand;
                    clockHand = (clockHand + 1) % BUFFER_SIZE;
                    return victimIdx;
                } else {
                    refBit[clockHand] = false;
                    clockHand = (clockHand + 1) % BUFFER_SIZE;
                }
            }
        }

        } // end switch
        return 0;
    }

public:
    // ── Construtor ────────────────────────────────────────────────────────────
    BufferManager(const string& file, Policy p)
        : filename(file), policy(p)
    {
        srand((unsigned)time(nullptr));
        refBit.assign(BUFFER_SIZE, false);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Evict(): remove uma entrada do buffer conforme a política
    // ─────────────────────────────────────────────────────────────────────────
    void Evict() {
        if (buffer.empty()) return;

        int idx = selectVictim();
        Page& victim = buffer[idx];

        printf("[EVICT] Página %d removida | Conteúdo: \"%s\"%s\n",
               victim.pageId,
               victim.content.c_str(),
               victim.dirty ? " W" : "");
        fflush(stdout);

        removeFromAux(victim.pageId);

        if (policy == CLOCK_POL) {
            if (clockHand > idx)
                clockHand--;
            else if (clockHand == (int)buffer.size() - 1)
                clockHand = 0;
            refBit.erase(refBit.begin() + idx);
            refBit.push_back(false);
        }

        buffer.erase(buffer.begin() + idx);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Fetch(int key): busca a página 'key'; usa buffer ou arquivo
    // ─────────────────────────────────────────────────────────────────────────
    void Fetch(int key) {
        int idx = findInBuffer(key);

        if (idx != -1) {
            // ── Cache HIT ──────────────────────────────────────────────────
            cacheHit++;
            printf("[FETCH] Cache HIT  | Página %d → \"%s\"\n",
                   key, buffer[idx].content.c_str());
            fflush(stdout);

            if (policy == LRU || policy == MRU)
                recordAccess(key);
            if (policy == CLOCK_POL)
                refBit[idx] = true;

        } else {
            // ── Cache MISS ─────────────────────────────────────────────────
            cacheMiss++;
            string line = readFromFile(key);
            if (line.empty()) {
                printf("[FETCH] Página %d não encontrada no arquivo.\n", key);
                fflush(stdout);
                return;
            }

            printf("[FETCH] Cache MISS | Página %d lida do arquivo → \"%s\"\n",
                   key, line.c_str());
            fflush(stdout);

            // Se buffer cheio, evict antes de inserir
            if ((int)buffer.size() >= BUFFER_SIZE)
                Evict();

            Page entry;
            entry.pageId  = key;
            entry.content = line;
            entry.dirty   = randomDirty();

            buffer.push_back(entry);
            int newIdx = (int)buffer.size() - 1;

            if (policy == LRU || policy == MRU)
                recordAccess(key);
            if (policy == FIFO)
                recordInsert(key);
            if (policy == CLOCK_POL)
                refBit[newIdx] = true;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // DisplayCache(): exibe o estado atual do buffer
    // ─────────────────────────────────────────────────────────────────────────
    void DisplayCache() {
        printf("\n╔══════════════════════════════════════════════════════╗\n");
        printf("║                   ESTADO DO BUFFER                  ║\n");
        printf("╠═══════╦═══════════════════════════════════╦══════════╣\n");
        printf("║ Chave ║ Valor (conteúdo)                  ║ Dirty    ║\n");
        printf("╠═══════╬═══════════════════════════════════╬══════════╣\n");
        if (buffer.empty()) {
            printf("║             (buffer vazio)                          ║\n");
        } else {
            for (auto& e : buffer) {
                string disp = e.content;
                if (disp.size() > 33) disp = disp.substr(0, 30) + "...";
                printf("║ %-5d ║ %-33s ║ %-8s ║\n",
                       e.pageId,
                       disp.c_str(),
                       e.dirty ? "TRUE" : "FALSE");
            }
        }
        printf("╚═══════╩═══════════════════════════════════╩══════════╝\n\n");
        fflush(stdout);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // DisplayStats(): exibe contagens de hit e miss
    // ─────────────────────────────────────────────────────────────────────────
    void DisplayStats() {
        int total = cacheHit + cacheMiss;
        double hitRate  = total > 0 ? 100.0 * cacheHit  / total : 0.0;
        double missRate = total > 0 ? 100.0 * cacheMiss / total : 0.0;
        printf("\n┌─────────────────────────────────┐\n");
        printf("│         ESTATÍSTICAS            │\n");
        printf("├─────────────────────────────────┤\n");
        printf("│  Cache HIT  : %-5d  (%.1f%%)   │\n", cacheHit,  hitRate);
        printf("│  Cache MISS : %-5d  (%.1f%%)   │\n", cacheMiss, missRate);
        printf("│  Total      : %-5d            │\n", total);
        printf("└─────────────────────────────────┘\n\n");
        fflush(stdout);
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Utilitário: converte string de política para enum
// ═══════════════════════════════════════════════════════════════════════════════
Policy parsePolicy(const string& s) {
    if (s == "LRU")   return LRU;
    if (s == "MRU")   return MRU;
    if (s == "FIFO")  return FIFO;
    if (s == "CLOCK") return CLOCK_POL;
    fprintf(stderr, "[ERRO] Política inválida. Use: LRU | FIFO | CLOCK | MRU\n");
    exit(1);
}

// ═══════════════════════════════════════════════════════════════════════════════
// main()
// ═══════════════════════════════════════════════════════════════════════════════
int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <arquivo.txt> <LRU|FIFO|CLOCK|MRU>\n", argv[0]);
        return 1;
    }

    string filename = argv[1];
    Policy policy   = parsePolicy(argv[2]);

    printf("Buffer Manager iniciado.\n");
    printf("Arquivo  : %s\n", filename.c_str());
    printf("Política : %s\n", argv[2]);
    printf("Tamanho máximo do buffer: %d\n\n", BUFFER_SIZE);

    BufferManager bm(filename, policy);

    // ── Menu interativo ───────────────────────────────────────────────────────
    string cmd;
    while (true) {
        printf("─────────────────────────────────────────────────────\n");
        printf("Comandos: fetch <n> | display | stats | quit\n> ");
        fflush(stdout);

        if (!(cin >> cmd)) break; // EOF

        if (cmd == "fetch") {
            int key;
            if (cin >> key)
                bm.Fetch(key);
        } else if (cmd == "display") {
            bm.DisplayCache();
        } else if (cmd == "stats") {
            bm.DisplayStats();
        } else if (cmd == "quit") {
            bm.DisplayCache();
            bm.DisplayStats();
            break;
        } else {
            printf("Comando desconhecido.\n");
        }
    }

    return 0;
}