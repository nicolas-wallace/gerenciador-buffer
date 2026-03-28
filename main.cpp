/*
 * Buffer Manager - Gerenciador de Buffer com políticas LRU, FIFO, CLOCK e MRU
 *
 * Compilação: g++ -o buffer_manager main.cpp
 * Uso: ./buffer_manager <arquivo_texto> <política: LRU|FIFO|CLOCK|MRU>
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
struct BufferEntry {
    int    pageId;      // Identificador da página (page#)
    std::string content;// Conteúdo da linha de texto
    bool   dirty;       // Variável de atualização (TRUE = foi modificada)
};

// ─── Políticas disponíveis ─────────────────────────────────────────────────────
enum Policy { LRU, FIFO, CLOCK_POL, MRU };

// ═══════════════════════════════════════════════════════════════════════════════
// Classe BufferManager
// ═══════════════════════════════════════════════════════════════════════════════
class BufferManager {
private:
    // --- Estado do buffer ---
    vector<BufferEntry> buffer;       // Entradas em memória
    int  cacheHit  = 0;
    int  cacheMiss = 0;
    Policy policy;

    // --- Estruturas auxiliares por política ---

    // LRU / MRU: lista de acesso ordenada por tempo de uso
    //   front = mais recentemente usado, back = menos recentemente usado
    list<int> lruOrder; // armazena pageId na ordem de acesso

    // FIFO: lista de inserção
    list<int> fifoOrder; // front = mais antigo

    // CLOCK: ponteiro circular + bit de referência por posição no buffer
    int  clockHand = 0;
    vector<bool> refBit; // bit de referência para cada slot do buffer

    // --- Arquivo de texto ---
    string filename;

    // ── Utilitários internos ──────────────────────────────────────────────────

    // Verifica se uma página está no buffer; retorna índice ou -1
    int findInBuffer(int key) {
        for (int i = 0; i < (int)buffer.size(); i++)
            if (buffer[i].pageId == key) return i;
        return -1;
    }

    // Lê a linha de número 'key' (1-indexed) do arquivo CSV
    // Remove aspas duplas ao redor do conteúdo, se presentes
    string readFromFile(int key) {
        ifstream file(filename);
        if (!file.is_open()) {
            cerr << "[ERRO] Não foi possível abrir o arquivo: " << filename << "\n";
            return "";
        }
        string line;
        int lineNum = 0;
        while (getline(file, line)) {
            lineNum++;
            if (lineNum == key) {
                // Remove aspas duplas no início e no fim, se existirem
                if (line.size() >= 2 && line.front() == '"' && line.back() == '"')
                    line = line.substr(1, line.size() - 2);
                return line;
            }
        }
        return ""; // página não encontrada
    }

    // Gera valor booleano aleatório para o bit dirty
    bool randomDirty() {
        return (rand() % 2) == 1;
    }

    // ── Lógica de ordem por política ─────────────────────────────────────────

    // Registra acesso a uma página (para LRU e MRU)
    void recordAccess(int pageId) {
        lruOrder.remove(pageId);
        lruOrder.push_front(pageId); // frente = mais recente
    }

    // Registra inserção de uma página (para FIFO)
    void recordInsert(int pageId) {
        fifoOrder.push_back(pageId); // fundo = mais novo → frente = mais velho
    }

    // Remove uma chave das listas auxiliares (ao fazer evict)
    void removeFromAux(int pageId) {
        lruOrder.remove(pageId);
        fifoOrder.remove(pageId);
    }

    // ── Seleção de vítima por política ────────────────────────────────────────

    // Retorna o índice no buffer da entrada a ser removida
    int selectVictim() {
        switch (policy) {

        case LRU: {
            // A vítima é a página menos recentemente usada (fundo da lista)
            int victim = lruOrder.back();
            return findInBuffer(victim);
        }

        case MRU: {
            // A vítima é a página mais recentemente usada (frente da lista)
            int victim = lruOrder.front();
            return findInBuffer(victim);
        }

        case FIFO: {
            // A vítima é a página mais antiga (frente da lista de inserção)
            int victim = fifoOrder.front();
            return findInBuffer(victim);
        }

        case CLOCK_POL: {
            // Ponteiro percorre circularmente; zera bit de referência até achar 0
            while (true) {
                if (!refBit[clockHand]) {
                    // Sem referência recente → é a vítima
                    int victimIdx = clockHand;
                    clockHand = (clockHand + 1) % BUFFER_SIZE;
                    return victimIdx;
                } else {
                    // Dá segunda chance: zera o bit e avança
                    refBit[clockHand] = false;
                    clockHand = (clockHand + 1) % BUFFER_SIZE;
                }
            }
        }

        } // end switch
        return 0; // fallback (nunca alcançado)
    }

public:
    // ── Construtor ────────────────────────────────────────────────────────────
    BufferManager(const string& file, Policy p)
        : filename(file), policy(p)
    {
        srand((unsigned)time(nullptr));
        // Inicializa vetor de bits de referência (CLOCK)
        refBit.assign(BUFFER_SIZE, false);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Evict(): remove uma entrada do buffer conforme a política
    // ─────────────────────────────────────────────────────────────────────────
    void Evict() {
        if (buffer.empty()) return;

        int idx = selectVictim();
        BufferEntry& victim = buffer[idx];

        // Exibe a página removida
        cout << "[EVICT] Página " << victim.pageId
             << " removida | Conteúdo: \"" << victim.content << "\"";
        if (victim.dirty)
            cout << " W"; // página suja → teria de ser escrita em disco
        cout << "\n";

        // Remove das estruturas auxiliares
        removeFromAux(victim.pageId);

        // Remove do buffer (shift do vetor; ajusta clockHand se necessário)
        if (policy == CLOCK_POL) {
            // Após remover o slot 'idx', entradas após ele deslocam uma posição
            // Ajusta o ponteiro do clock para não pular nem repetir
            if (clockHand > idx)
                clockHand--;
            else if (clockHand == (int)buffer.size() - 1)
                clockHand = 0;
            refBit.erase(refBit.begin() + idx);
            refBit.push_back(false); // novo slot ao final (livre)
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
            cout << "[FETCH] Cache HIT  | Página " << key
                 << " → \"" << buffer[idx].content << "\"\n";

            // Atualiza estruturas de ordem
            if (policy == LRU || policy == MRU)
                recordAccess(key);
            if (policy == CLOCK_POL)
                refBit[idx] = true; // renova referência

        } else {
            // ── Cache MISS ─────────────────────────────────────────────────
            cacheMiss++;
            string line = readFromFile(key);
            if (line.empty()) {
                cout << "[FETCH] Página " << key << " não encontrada no arquivo.\n";
                return;
            }
            cout << "[FETCH] Cache MISS | Página " << key
                 << " lida do arquivo → \"" << line << "\"\n";

            // Se buffer cheio, evict antes de inserir
            if ((int)buffer.size() >= BUFFER_SIZE)
                Evict();

            // Cria nova entrada
            BufferEntry entry;
            entry.pageId  = key;
            entry.content = line;
            entry.dirty   = randomDirty();

            buffer.push_back(entry);
            int newIdx = (int)buffer.size() - 1;

            // Registra nas estruturas auxiliares
            if (policy == LRU || policy == MRU)
                recordAccess(key);
            if (policy == FIFO)
                recordInsert(key);
            if (policy == CLOCK_POL)
                refBit[newIdx] = true; // recém carregada → referência ativa
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // DisplayCache(): exibe o estado atual do buffer
    // ─────────────────────────────────────────────────────────────────────────
    void DisplayCache() {
        cout << "\n╔══════════════════════════════════════════════════════╗\n";
        cout << "║                   ESTADO DO BUFFER                  ║\n";
        cout << "╠═══════╦═══════════════════════════════════╦══════════╣\n";
        cout << "║ Chave ║ Valor (conteúdo)                  ║ Dirty    ║\n";
        cout << "╠═══════╬═══════════════════════════════════╬══════════╣\n";
        if (buffer.empty()) {
            cout << "║             (buffer vazio)                          ║\n";
        } else {
            for (auto& e : buffer) {
                // Trunca conteúdo para caber na tabela
                string disp = e.content;
                if (disp.size() > 33) disp = disp.substr(0, 30) + "...";
                printf("║ %-5d ║ %-33s ║ %-8s ║\n",
                       e.pageId,
                       disp.c_str(),
                       e.dirty ? "TRUE" : "FALSE");
            }
        }
        cout << "╚═══════╩═══════════════════════════════════╩══════════╝\n\n";
    }

    // ─────────────────────────────────────────────────────────────────────────
    // DisplayStats(): exibe contagens de hit e miss
    // ─────────────────────────────────────────────────────────────────────────
    void DisplayStats() {
        int total = cacheHit + cacheMiss;
        double hitRate  = total > 0 ? 100.0 * cacheHit  / total : 0.0;
        double missRate = total > 0 ? 100.0 * cacheMiss / total : 0.0;
        cout << "\n┌─────────────────────────────────┐\n";
        cout << "│         ESTATÍSTICAS            │\n";
        cout << "├─────────────────────────────────┤\n";
        printf("│  Cache HIT  : %-5d  (%.1f%%)   │\n", cacheHit,  hitRate);
        printf("│  Cache MISS : %-5d  (%.1f%%)   │\n", cacheMiss, missRate);
        printf("│  Total      : %-5d            │\n", total);
        cout << "└─────────────────────────────────┘\n\n";
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
    std::cerr << "[ERRO] Política inválida. Use: LRU | FIFO | CLOCK | MRU\n";
    exit(1);
}

// ═══════════════════════════════════════════════════════════════════════════════
// main(): demonstra o gerenciador com sequência de acessos interativa
// ═══════════════════════════════════════════════════════════════════════════════
int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Uso: " << argv[0] << " <arquivo.txt> <LRU|FIFO|CLOCK|MRU>\n";
        return 1;
    }

    std::string filename = argv[1];
    Policy policy = parsePolicy(argv[2]);

    std::cout << "Buffer Manager iniciado.\n"
              << "Arquivo  : " << filename  << "\n"
              << "Política : " << argv[2]   << "\n"
              << "Tamanho máximo do buffer: " << BUFFER_SIZE << "\n\n";

    BufferManager bm(filename, policy);

    // ── Menu interativo ───────────────────────────────────────────────────────
    std::string cmd;
    while (true) {
        std::cout << "─────────────────────────────────────────────────────\n";
        std::cout << "Comandos: fetch <n> | display | stats | quit\n> ";
        std::cin >> cmd;

        if (cmd == "fetch") {
            int key;
            std::cin >> key;
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
            std::cout << "Comando desconhecido.\n";
        }
    }

    return 0;
}