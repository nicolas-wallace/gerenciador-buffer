# Gerenciador de Buffer
Implementação de um gerenciador de buffer em C++ com quatro políticas de substituição de página: **LRU**, **FIFO**, **CLOCK** e **MRU**.
O buffer mantém até 5 páginas em memória. Cada página corresponde a uma linha de um arquivo CSV. Quando o buffer está cheio e uma nova página é solicitada, uma entrada é removida conforme a política escolhida.

---

## Estrutura do projeto

```
/src/main.cpp   ← código fonte
/bin/           ← binário gerado pela compilação
Makefile
```

---

## Compilação

```bash
make
```

O binário é gerado em `/bin/buffer_manager`.

---

## Uso

```bash
make run FILE=<arquivo.csv> POLICY=<POLÍTICA>
```

**Exemplo:**

```bash
make run FILE=bancodedados.csv POLICY=LRU
```

Ou diretamente após compilar:

```bash
./bin/buffer_manager <arquivo.csv> <POLÍTICA>
```

---

## Targets do Makefile

| Target  | Descrição                                              |
|---------|--------------------------------------------------------|
| `all`   | Compila o projeto (padrão)                             |
| `run`   | Compila e executa — aceita `FILE=` e `POLICY=`         |
| `clean` | Remove o binário gerado                                |

---

## Políticas disponíveis

| Política | Descrição                                                      |
|----------|----------------------------------------------------------------|
| `LRU`    | Remove a página menos recentemente usada                       |
| `MRU`    | Remove a página mais recentemente usada                        |
| `FIFO`   | Remove a página carregada há mais tempo                        |
| `CLOCK`  | Ponteiro circular com bit de referência (segunda chance)       |

---

## Comandos interativos

| Comando      | Descrição                                                         |
|--------------|-------------------------------------------------------------------|
| `fetch <n>`  | Busca a página de número `n` (linha `n` do arquivo)               |
| `display`    | Exibe o estado atual do buffer                                    |
| `stats`      | Exibe contadores de cache hit e cache miss                        |
| `quit`       | Encerra o programa exibindo estado final e estatísticas           |

---

## Formato do arquivo CSV

Cada linha do arquivo corresponde a uma página. O número da linha é o identificador da página (`page#`). O programa aceita arquivos com line endings Unix (`LF`) e Windows (`CRLF`), e linhas com ou sem aspas duplas.

```
"Conteúdo da página 1"
"Conteúdo da página 2"
Conteúdo sem aspas também é aceito
```

---

## Funcionamento

- **Cache hit:** a página solicitada já está no buffer.
- **Cache miss:** a página não está no buffer e é lida do arquivo.
- **Dirty bit:** atribuído aleatoriamente no momento da carga. Se `TRUE`, a página exibe `W` ao ser removida (indicando que precisaria ser escrita em disco).