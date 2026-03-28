# Gerenciador de Buffer

Implementação de um gerenciador de buffer em C++ com quatro políticas de substituição de página: **LRU**, **FIFO**, **CLOCK** e **MRU**.

O buffer mantém até 5 páginas em memória. Cada página corresponde a uma linha de um arquivo CSV. Quando o buffer está cheio e uma nova página é solicitada, uma entrada é removida conforme a política escolhida.

---

## Compilação

```bash
g++ -o buffer_manager main.cpp
```

## Uso

```bash
./buffer_manager <arquivo.csv> <POLÍTICA>
```

**Exemplo:**
```bash
./buffer_manager bancodedados.csv LRU
```

---

## Políticas disponíveis

| Política | Descrição |
|----------|-----------|
| `LRU`    | Remove a página menos recentemente usada |
| `MRU`    | Remove a página mais recentemente usada |
| `FIFO`   | Remove a página carregada há mais tempo |
| `CLOCK`  | Ponteiro circular com bit de referência (segunda chance) |

---

## Comandos interativos

| Comando      | Descrição |
|--------------|-----------|
| `fetch <n>`  | Busca a página de número `n` (linha `n` do arquivo) |
| `display`    | Exibe o estado atual do buffer |
| `stats`      | Exibe contadores de cache hit e cache miss |
| `quit`       | Encerra o programa exibindo estado final e estatísticas |

---

## Formato do arquivo CSV

Cada linha do arquivo corresponde a uma página. O número da linha é o identificador da página (`page#`). O programa aceita linhas com ou sem aspas duplas.

```
"Conteúdo da página 1"
"Conteúdo da página 2"
```

---

## Funcionamento

- **Cache hit:** a página solicitada já está no buffer.
- **Cache miss:** a página não está no buffer e é lida do arquivo.
- **Dirty bit:** atribuído aleatoriamente no momento da carga. Se `TRUE`, a página exibe `W` ao ser removida (indicando que precisaria ser escrita em disco).
