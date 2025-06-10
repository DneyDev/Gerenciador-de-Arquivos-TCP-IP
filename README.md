
# 🗂 Projeto de Gerenciamento de Arquivos via Sockets TCP (Winsock)

## 📌 Descrição

Este projeto implementa uma aplicação cliente-servidor em C usando Winsock (Windows Sockets API), com o objetivo de realizar **operações remotas em arquivos** via TCP. O cliente envia comandos ao servidor para listar, baixar, enviar ou remover arquivos em diretórios do servidor. Tudo isso com um menu interativo no terminal.

---

## 🚀 Funcionalidades

**Cliente (`ClienteWinsockTCP.c`):**
- Interface de linha de comando para o usuário
- Menu interativo
- Envio de comandos estruturados para o servidor
- Exibe progresso de envio/recebimento de arquivos

**Servidor (`ServidorWinsockTCP.c`):**
- Aceita múltiplas conexões simultâneas via threads
- Executa comandos de gerenciamento de arquivos:
  - `L` - Listar diretório
  - `D` - Download de arquivo
  - `U` - Upload de arquivo
  - `R` - Remoção de arquivo
- Comunicação robusta com o cliente usando a estrutura `ComandoArquivo`

---

## 🛠️ Estrutura do Protocolo

A comunicação entre cliente e servidor utiliza a estrutura `ComandoArquivo`:

```c
typedef struct {
    char comando;             // 'L', 'D', 'U', 'R'
    char caminho[MAX_PATH];   // Caminho do arquivo ou diretório
    DWORD tamanho;            // Tamanho do arquivo (para upload/download)
} ComandoArquivo;
```

---

## 🔧 Compilação

Você precisa do **Visual Studio** ou outro compilador C que suporte a API **Winsock2**.

**Compilação via linha de comando:**

```bash
cl /D_WIN32_WINNT=0x0601 ClienteWinsockTCP.c /link ws2_32.lib
cl /D_WIN32_WINNT=0x0601 ServidorWinsockTCP.c /link ws2_32.lib
```

---

## 📦 Executando

### 🖥️ Servidor
```bash
ServidorWinsockTCP.exe
```

Ele ficará escutando na porta padrão `27015`, aceitando múltiplas conexões.

### 💻 Cliente
```bash
ClienteWinsockTCP.exe 127.0.0.1
```

Você pode passar o IP do servidor como argumento (localhost por padrão).

---

## 📋 Exemplo de Uso

1. Rodar o servidor
2. Rodar o cliente
3. Escolher uma das opções do menu:
   - Listar arquivos: informe um diretório remoto
   - Baixar: informe caminho remoto e nome local para salvar
   - Enviar: informe arquivo local e caminho remoto no servidor
   - Remover: informe o caminho do arquivo no servidor

---

## 🧠 Conceitos Envolvidos

- Sockets TCP
- API Winsock (Windows)
- Threads no servidor para atender múltiplos clientes
- I/O de arquivos em modo binário
- Protocolos personalizados para troca de mensagens
- Bufferização e controle de fluxo simples

---

## ⚠️ Requisitos

- Windows com Winsock 2.2+
- Compilador C compatível com Windows API
- Permissão de leitura/escrita nos diretórios usados

---

## 🧼 Observações

- O cliente e o servidor usam marcadores como `---FIM DA LISTAGEM---` e `---FIM DO ARQUIVO---` para indicar o fim da transmissão.
- O projeto foi escrito com portabilidade e segurança em mente: `fgets` no lugar de `gets`, uso de `strncpy`, verificação de erros, etc.
- Para múltiplas conexões, o servidor cria uma thread por cliente conectado.
