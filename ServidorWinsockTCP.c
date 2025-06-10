#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Definição para evitar conflitos de headers
#define WIN32_LEAN_AND_MEAN

// Certifique-se que a biblioteca Winsock seja linkada
#pragma comment(lib, "ws2_32.lib")

#define DEFAULT_PORT 27015
#define DEFAULT_BUFLEN 4096

typedef struct {
    char comando;         // Tipo de comando: 'L' (listar), 'D' (download), 'U' (upload), 'R' (remover)
    char caminho[MAX_PATH]; // Caminho do arquivo ou diretório
    DWORD tamanho;        // Tamanho do arquivo (para upload/download)
} ComandoArquivo;

// Função para listar arquivos de um diretório e enviar para o cliente
void listarArquivos(SOCKET clientSocket, const char* diretorio) {
    WIN32_FIND_DATAA findFileData;
    HANDLE hFind;
    char searchPath[MAX_PATH];
    char resposta[DEFAULT_BUFLEN] = {0};
    int bytesEnviados = 0;
    
    // Prepara o caminho para busca
    snprintf(searchPath, MAX_PATH, "%s\\*", diretorio);
    
    // Inicia a busca de arquivos
    hFind = FindFirstFileA(searchPath, &findFileData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        strncpy(resposta, "Erro ao listar diretorio\r\n", DEFAULT_BUFLEN - 1);
        resposta[DEFAULT_BUFLEN - 1] = '\0';  // garante terminação nula
        send(clientSocket, resposta, (int)strlen(resposta), 0);
        return;
    }
    
    // Limpa o buffer de resposta
    memset(resposta, 0, DEFAULT_BUFLEN);
    
    // Adiciona cabeçalho da listagem
    strncat(resposta, "Listagem de arquivos:\r\n", DEFAULT_BUFLEN - strlen(resposta) - 1);
    
    // Processa cada arquivo encontrado
    do {
        // Ignora os diretórios especiais . e ..
        if (strcmp(findFileData.cFileName, ".") != 0 && strcmp(findFileData.cFileName, "..") != 0) {
            char tipoArquivo[10];
            
            // Determina se é um diretório ou arquivo
            if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                strncpy(tipoArquivo, "[DIR]", 10);
            } else {
                strncpy(tipoArquivo, "[ARQ]", 10);
                tipoArquivo[9] = '\0'; // Garantir terminação nula
            }
            
            // Adiciona informação do arquivo à resposta
            char linhaArquivo[MAX_PATH + 20];
            snprintf(linhaArquivo, MAX_PATH + 20, "%s %s\r\n", tipoArquivo, findFileData.cFileName);
            
            // Verifica se o buffer está quase cheio
            if (strlen(resposta) + strlen(linhaArquivo) >= DEFAULT_BUFLEN - 1) {
                // Envia o buffer atual
                bytesEnviados = send(clientSocket, resposta, (int)strlen(resposta), 0);
                if (bytesEnviados == SOCKET_ERROR) {
                    printf("Erro ao enviar dados: %d\n", WSAGetLastError());
                    break;
                }
                
                // Limpa o buffer para a próxima parte
                memset(resposta, 0, DEFAULT_BUFLEN);
            }
            
            // Adiciona a linha ao buffer
            strncat(resposta, linhaArquivo, DEFAULT_BUFLEN - strlen(resposta) - 1);
        }
    } while (FindNextFileA(hFind, &findFileData) != 0);
    
    // Envia o restante dos dados
    if (strlen(resposta) > 0) {
        bytesEnviados = send(clientSocket, resposta, (int)strlen(resposta), 0);
        if (bytesEnviados == SOCKET_ERROR) {
            printf("Erro ao enviar dados: %d\n", WSAGetLastError());
        }
    }
    
    // Envia marcador de fim de listagem
    strncpy(resposta, "---FIM DA LISTAGEM---\r\n", DEFAULT_BUFLEN - 1);
    resposta[DEFAULT_BUFLEN - 1] = '\0'; // Garantir terminação nula
    send(clientSocket, resposta, (int)strlen(resposta), 0);
    
    FindClose(hFind);
}

// Função para enviar um arquivo para o cliente
void enviarArquivo(SOCKET clientSocket, const char* caminhoArquivo) {
    FILE* arquivo;
    char buffer[DEFAULT_BUFLEN];
    int bytesLidos, bytesEnviados;
    DWORD tamanhoArquivo = 0;
    
    // Abre o arquivo em modo binário
    arquivo = fopen(caminhoArquivo, "rb");
    if (arquivo == NULL) {
        snprintf(buffer, DEFAULT_BUFLEN, "ERRO: Nao foi possivel abrir o arquivo %s\r\n", caminhoArquivo);
        send(clientSocket, buffer, (int)strlen(buffer), 0);
        return;
    }
    
    // Obtém o tamanho do arquivo
    fseek(arquivo, 0, SEEK_END);
    tamanhoArquivo = ftell(arquivo);
    fseek(arquivo, 0, SEEK_SET);
    
    // Envia o tamanho do arquivo
    snprintf(buffer, DEFAULT_BUFLEN, "TAMANHO:%u\r\n", tamanhoArquivo);
    send(clientSocket, buffer, (int)strlen(buffer), 0);
    
    // Envia o conteúdo do arquivo
    while ((bytesLidos = (int)fread(buffer, 1, DEFAULT_BUFLEN, arquivo)) > 0) {
        bytesEnviados = send(clientSocket, buffer, bytesLidos, 0);
        if (bytesEnviados == SOCKET_ERROR) {
            printf("Erro ao enviar dados: %d\n", WSAGetLastError());
            break;
        }
    }
    
    fclose(arquivo);
    
    // Envia marcador de fim de arquivo
    strncpy(buffer, "---FIM DO ARQUIVO---\r\n", DEFAULT_BUFLEN - 1);
    buffer[DEFAULT_BUFLEN - 1] = '\0'; // Garantir terminação nula
    send(clientSocket, buffer, (int)strlen(buffer), 0);
}

// Função para receber um arquivo do cliente
void receberArquivo(SOCKET clientSocket, const char* caminhoArquivo, DWORD tamanhoArquivo) {
    FILE* arquivo;
    char buffer[DEFAULT_BUFLEN];
    int bytesRecebidos;
    DWORD bytesRestantes = tamanhoArquivo;
    
    // Abre o arquivo em modo binário para escrita - CORRIGIDO
    arquivo = fopen(caminhoArquivo, "wb"); // "wb" é para ESCRITA binária
    if (arquivo == NULL) {
        snprintf(buffer, DEFAULT_BUFLEN, "ERRO: Não foi possível criar o arquivo %s\r\n", caminhoArquivo);
        send(clientSocket, buffer, (int)strlen(buffer), 0);
        return;
    }
    
    // Envia confirmação para iniciar a transferência
    strncpy(buffer, "PRONTO_PARA_RECEBER\r\n", DEFAULT_BUFLEN - 1);
    buffer[DEFAULT_BUFLEN - 1] = '\0'; // Garantir terminação nula
    send(clientSocket, buffer, (int)strlen(buffer), 0);
    
    // Recebe o conteúdo do arquivo
    while (bytesRestantes > 0) {
        int bytesAReceber = (bytesRestantes < DEFAULT_BUFLEN) ? bytesRestantes : DEFAULT_BUFLEN;
        
        bytesRecebidos = recv(clientSocket, buffer, bytesAReceber, 0);
        if (bytesRecebidos <= 0) {
            printf("Erro ao receber dados ou conexão fechada: %d\n", WSAGetLastError());
            break;
        }
        
        // Escreve os dados recebidos no arquivo
        fwrite(buffer, 1, bytesRecebidos, arquivo);
        bytesRestantes -= bytesRecebidos;
    }
    
    fclose(arquivo);
    
    // Envia confirmação de recebimento
    strncpy(buffer, "ARQUIVO_RECEBIDO\r\n", DEFAULT_BUFLEN - 1);
    buffer[DEFAULT_BUFLEN - 1] = '\0'; // Garantir terminação nula
    send(clientSocket, buffer, (int)strlen(buffer), 0);
}

// Função para remover um arquivo
void removerArquivo(SOCKET clientSocket, const char* caminhoArquivo) {
    char buffer[DEFAULT_BUFLEN];
    
    if (DeleteFileA(caminhoArquivo)) {
        snprintf(buffer, DEFAULT_BUFLEN, "Arquivo %s removido com sucesso\r\n", caminhoArquivo);
    } else {
        snprintf(buffer, DEFAULT_BUFLEN, "ERRO: Não foi possível remover o arquivo %s (Código: %d)\r\n", 
                 caminhoArquivo, GetLastError());
    }
    
    send(clientSocket, buffer, (int)strlen(buffer), 0);
}

// Função para processar comandos do cliente
void processarComando(SOCKET clientSocket, ComandoArquivo* comando) {
    switch (comando->comando) {
        case 'L': // Listar arquivos
            listarArquivos(clientSocket, comando->caminho);
            break;
            
        case 'D': // Download (enviar arquivo para o cliente)
            enviarArquivo(clientSocket, comando->caminho);
            break;
            
        case 'U': // Upload (receber arquivo do cliente)
            receberArquivo(clientSocket, comando->caminho, comando->tamanho);
            break;
                    case 'R': // Remover arquivo
            removerArquivo(clientSocket, comando->caminho);
            break;

        case 'M': // Receber Mensagem
            // Garante que a string recebida seja nula terminada dentro dos limites
            comando->caminho[MAX_PATH - 1] = '\0';
            printf("Mensagem recebida do cliente: \"%s\" (Tamanho: %u)\n", comando->caminho, comando->tamanho);
            // Envia uma confirmação simples de volta
            {
                char respostaMsg[] = "Mensagem recebida com sucesso!\r\n";
                send(clientSocket, respostaMsg, (int)strlen(respostaMsg), 0);
            }
            break;

        default:     
            // Comando desconhecido
            {
                char resposta[100];
                snprintf(resposta, 100, "Comando desconhecido: %c\r\n", comando->comando);
                send(clientSocket, resposta, (int)strlen(resposta), 0);
            }
            break;
    }
}

// Função para tratar cada cliente conectado
DWORD WINAPI tratarCliente(LPVOID lpParam) {
    SOCKET clientSocket = (SOCKET)lpParam;
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;
    int bytesRecebidos;
    
    // Recebe dados do cliente
    while (1) {
        bytesRecebidos = recv(clientSocket, recvbuf, recvbuflen, 0);
        if (bytesRecebidos > 0) {
            // Processa o comando recebido
            ComandoArquivo* comando = (ComandoArquivo*)recvbuf;
            processarComando(clientSocket, comando);
        } else if (bytesRecebidos == 0) {
            // Conexão fechada pelo cliente
            printf("Conexão fechada pelo cliente\n");
            break;
        } else {
            // Erro na recepção
            printf("Erro na recepcao: %d\n", WSAGetLastError());
            break;
        }
    }
    
    // Fecha o socket do cliente
    closesocket(clientSocket);
    return 0;
}

// Implementação alternativa sem usar getaddrinfo/freeaddrinfo
int main(int argc, char* argv[]) {
    WSADATA wsaData;
    SOCKET listenSocket = INVALID_SOCKET;
    SOCKET clientSocket = INVALID_SOCKET;
    struct sockaddr_in serverAddr;
    int iResult;
    DWORD threadId;
    
    // Inicializa Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup falhou: %d\n", iResult);
        return 1;
    }
    
    // Cria o socket para escutar conexões
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        printf("Erro ao criar socket: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    
    // Configura o endereço do servidor
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY; // Aceita conexões de qualquer endereço
    serverAddr.sin_port = htons(DEFAULT_PORT); // Converte para network byte order
    
    // Associa o socket ao endereço IP e porta
    iResult = bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (iResult == SOCKET_ERROR) {
        printf("Erro no bind: %d\n", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }
    
    // Inicia a escuta por conexões
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        printf("Erro no listen: %d\n", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }
    
    printf("Servidor iniciado. Aguardando conexões na porta %d...\n", DEFAULT_PORT);
    
    // Loop principal para aceitar conexões
    while (1) {
        // Aceita uma conexão de cliente
        clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            printf("Erro no accept: %d\n", WSAGetLastError());
            continue;
        }
        
        printf("Nova conexao aceita\n");
        
        // Cria uma thread para tratar o cliente
        HANDLE hThread = CreateThread(NULL, 0, tratarCliente, (LPVOID)clientSocket, 0, &threadId);
        if (hThread == NULL) {
            printf("Erro ao criar thread: %d\n", GetLastError());
            closesocket(clientSocket);
        } else {
            // Fecha o handle da thread, mas a thread continua executando
            CloseHandle(hThread);
        }
    }
    
    // Limpa
    closesocket(listenSocket);
    WSACleanup();
    
    return 0;
}
