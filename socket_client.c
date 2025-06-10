#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 4096

typedef struct {
    char comando;         // Tipo de comando: 'L' (listar), 'D' (download), 'U' (upload), 'R' (remover)
    char caminho[MAX_PATH]; // Caminho do arquivo ou diretório
    DWORD tamanho;        // Tamanho do arquivo (para upload/download)
} ComandoArquivo;

// Função para listar arquivos do servidor
void listarArquivos(SOCKET serverSocket, const char* diretorio) {
    ComandoArquivo comando;
    char buffer[DEFAULT_BUFLEN];
    int bytesRecebidos;
    
    // Prepara o comando
    comando.comando = 'L';
    strcpy_s(comando.caminho, MAX_PATH, diretorio);
    comando.tamanho = 0;
    
    // Envia o comando para o servidor
    send(serverSocket, (char*)&comando, sizeof(ComandoArquivo), 0);
    
    // Recebe e exibe a resposta do servidor
    printf("Listagem de arquivos em %s:\n", diretorio);
    printf("------------------------------------------\n");
    
    while (1) {
        bytesRecebidos = recv(serverSocket, buffer, DEFAULT_BUFLEN, 0);
        if (bytesRecebidos > 0) {
            // Garante que o buffer termine com null
            buffer[bytesRecebidos] = '\0';
            
            // Verifica se é o fim da listagem
            if (strstr(buffer, "---FIM DA LISTAGEM---") != NULL) {
                break;
            }
            
            // Exibe os dados recebidos
            printf("%s", buffer);
        } else if (bytesRecebidos == 0) {
            printf("Conexão fechada pelo servidor\n");
            break;
        } else {
            printf("Erro na recepcao: %d\n", WSAGetLastError());
            break;
        }
    }
    
    printf("------------------------------------------\n");
}

// Função para baixar um arquivo do servidor
void baixarArquivo(SOCKET serverSocket, const char* caminhoRemoto, const char* caminhoLocal) {
    ComandoArquivo comando;
    char buffer[DEFAULT_BUFLEN];
    int bytesRecebidos;
    FILE* arquivo = NULL;
    DWORD tamanhoArquivo = 0;
    DWORD bytesRecebidosTotal = 0;
    
    // Prepara o comando
    comando.comando = 'D';
    strcpy_s(comando.caminho, MAX_PATH, caminhoRemoto);
    comando.tamanho = 0;
    
    // Envia o comando para o servidor
    send(serverSocket, (char*)&comando, sizeof(ComandoArquivo), 0);
    
    // Recebe a resposta inicial do servidor
    bytesRecebidos = recv(serverSocket, buffer, DEFAULT_BUFLEN, 0);
    if (bytesRecebidos <= 0) {
        printf("Erro ao receber resposta do servidor\n");
        return;
    }
    
    buffer[bytesRecebidos] = '\0';
    
    // Verifica se houve erro
    if (strncmp(buffer, "ERRO:", 5) == 0) {
        printf("%s", buffer);
        return;
    }
    
    // Extrai o tamanho do arquivo
    if (strncmp(buffer, "TAMANHO:", 8) == 0) {
        sscanf_s(buffer + 8, "%u", &tamanhoArquivo);
        printf("Tamanho do arquivo: %u bytes\n", tamanhoArquivo);
    } else {
        printf("Resposta inesperada do servidor\n");
        return;
    }
    
    // Abre o arquivo para escrita
    if (fopen_s(&arquivo, caminhoLocal, "wb") != 0) {
        printf("Erro ao criar o arquivo local: %s\n", caminhoLocal);
        return;
    }
    
    printf("Baixando arquivo...\n");
    
    // Recebe o conteúdo do arquivo
    while (bytesRecebidosTotal < tamanhoArquivo) {
        bytesRecebidos = recv(serverSocket, buffer, DEFAULT_BUFLEN, 0);
        if (bytesRecebidos <= 0) {
            printf("Erro ao receber dados ou conexão fechada\n");
            break;
        }
        
        // Verifica se é o marcador de fim de arquivo
        if (bytesRecebidosTotal + bytesRecebidos >= tamanhoArquivo) {
            // Procura pelo marcador de fim
            char* fimArquivo = strstr(buffer, "---FIM DO ARQUIVO---");
            if (fimArquivo != NULL) {
                // Calcula quantos bytes pertencem ao arquivo
                int bytesArquivo = (int)(fimArquivo - buffer);
                fwrite(buffer, 1, bytesArquivo, arquivo);
                bytesRecebidosTotal += bytesArquivo;
                break;
            }
        }
        
        // Escreve os dados no arquivo
        fwrite(buffer, 1, bytesRecebidos, arquivo);
        bytesRecebidosTotal += bytesRecebidos;
        
        // Exibe progresso
        printf("\rProgresso: %.1f%%", (bytesRecebidosTotal * 100.0) / tamanhoArquivo);
    }
    
    fclose(arquivo);
    printf("\nArquivo baixado com sucesso: %s\n", caminhoLocal);
}

// Função para enviar um arquivo para o servidor
void enviarArquivo(SOCKET serverSocket, const char* caminhoLocal, const char* caminhoRemoto) {
    ComandoArquivo comando;
    char buffer[DEFAULT_BUFLEN];
    int bytesRecebidos, bytesLidos, bytesEnviados;
    FILE* arquivo = NULL;
    DWORD tamanhoArquivo = 0;
    DWORD bytesEnviadosTotal = 0;
    
    // Abre o arquivo para leitura
    if (fopen_s(&arquivo, caminhoLocal, "rb") != 0) {
        printf("Erro ao abrir o arquivo local: %s\n", caminhoLocal);
        return;
    }
    
    // Obtém o tamanho do arquivo
    fseek(arquivo, 0, SEEK_END);
    tamanhoArquivo = ftell(arquivo);
    fseek(arquivo, 0, SEEK_SET);
    
    // Prepara o comando
    comando.comando = 'U';
    strcpy_s(comando.caminho, MAX_PATH, caminhoRemoto);
    comando.tamanho = tamanhoArquivo;
    
    // Envia o comando para o servidor
    send(serverSocket, (char*)&comando, sizeof(ComandoArquivo), 0);
    
    // Aguarda confirmação do servidor
    bytesRecebidos = recv(serverSocket, buffer, DEFAULT_BUFLEN, 0);
    if (bytesRecebidos <= 0) {
        printf("Erro ao receber resposta do servidor\n");
        fclose(arquivo);
        return;
    }
    
    buffer[bytesRecebidos] = '\0';
    
    // Verifica se o servidor está pronto para receber
    if (strcmp(buffer, "PRONTO_PARA_RECEBER\r\n") != 0) {
        printf("Servidor não está pronto para receber: %s\n", buffer);
        fclose(arquivo);
        return;
    }
    
    printf("Enviando arquivo...\n");
    
    // Envia o conteúdo do arquivo
    while ((bytesLidos = (int)fread(buffer, 1, DEFAULT_BUFLEN, arquivo)) > 0) {
        bytesEnviados = send(serverSocket, buffer, bytesLidos, 0);
        if (bytesEnviados == SOCKET_ERROR) {
            printf("Erro ao enviar dados: %d\n", WSAGetLastError());
            break;
        }
        
        bytesEnviadosTotal += bytesEnviados;
        
        // Exibe progresso
        printf("\rProgresso: %.1f%%", (bytesEnviadosTotal * 100.0) / tamanhoArquivo);
    }
    
    fclose(arquivo);
    
    // Recebe confirmação final
    bytesRecebidos = recv(serverSocket, buffer, DEFAULT_BUFLEN, 0);
    if (bytesRecebidos > 0) {
        buffer[bytesRecebidos] = '\0';
        printf("\n%s\n", buffer);
    }
    
    printf("\nArquivo enviado com sucesso: %s\n", caminhoRemoto);
}

// Função para remover um arquivo no servidor
void removerArquivo(SOCKET serverSocket, const char* caminhoRemoto) {
    ComandoArquivo comando;
    char buffer[DEFAULT_BUFLEN];
    int bytesRecebidos;
    
    // Prepara o comando
    comando.comando = 'R';
    strcpy_s(comando.caminho, MAX_PATH, caminhoRemoto);
    comando.tamanho = 0;
    
    // Envia o comando para o servidor
    send(serverSocket, (char*)&comando, sizeof(ComandoArquivo), 0);
    
    // Recebe a resposta do servidor
    bytesRecebidos = recv(serverSocket, buffer, DEFAULT_BUFLEN, 0);
    if (bytesRecebidos > 0) {
        buffer[bytesRecebidos] = '\0';
        printf("%s", buffer);
    } else {
        printf("Erro ao receber resposta do servidor\n");
    }
}

// Função para exibir o menu de opções
void exibirMenu() {
    printf("\n===== GERENCIADOR DE ARQUIVOS - CLIENTE =====\n");
    printf("1. Listar arquivos\n");
    printf("2. Baixar arquivo\n");
    printf("3. Enviar arquivo\n");
    printf("4. Remover arquivo\n");
    printf("0. Sair\n");
    printf("Escolha uma opção: ");
}

int main(int argc, char* argv[]) {
    WSADATA wsaData;
    SOCKET connectSocket = INVALID_SOCKET;
    struct addrinfo *result = NULL, *ptr = NULL, hints;
    int iResult;
    char serverIP[16] = "127.0.0.1"; // IP padrão (localhost)
    
    // Verifica se o IP do servidor foi fornecido
    if (argc > 1) {
        strcpy_s(serverIP, 16, argv[1]);
    }
    
    // Inicializa Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup falhou: %d\n", iResult);
        return 1;
    }
    
    // Configura as dicas para o endereço
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    
    // Resolve o endereço e porta do servidor
    iResult = getaddrinfo(serverIP, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo falhou: %d\n", iResult);
        WSACleanup();
        return 1;
    }
    
    // Tenta conectar ao servidor
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        // Cria o socket para conectar ao servidor
        connectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (connectSocket == INVALID_SOCKET) {
            printf("Erro ao criar socket: %d\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }
        
        // Conecta ao servidor
        iResult = connect(connectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(connectSocket);
            connectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }
    
    freeaddrinfo(result);
    
    if (connectSocket == INVALID_SOCKET) {
        printf("Não foi possível conectar ao servidor!\n");
        WSACleanup();
        return 1;
    }
    
    printf("Conectado ao servidor %s:%s\n", serverIP, DEFAULT_PORT);
    
    // Loop principal do cliente
    int opcao = -1;
    char caminhoRemoto[MAX_PATH];
    char caminhoLocal[MAX_PATH];
    
    while (opcao != 0) {
        exibirMenu();
        scanf_s("%d", &opcao);
        
        // Limpa o buffer de entrada
        while (getchar() != '\n');
        
        switch (opcao) {
            case 1: // Listar arquivos
                printf("Digite o diretório remoto (ou . para atual): ");
                gets_s(caminhoRemoto, MAX_PATH);
                listarArquivos(connectSocket, caminhoRemoto);
                break;
                
            case 2: // Baixar arquivo
                printf("Digite o caminho do arquivo remoto: ");
                gets_s(caminhoRemoto, MAX_PATH);
                printf("Digite o caminho para salvar localmente: ");
                gets_s(caminhoLocal, MAX_PATH);
                baixarArquivo(connectSocket, caminhoRemoto, caminhoLocal);
                break;
                
            case 3: // Enviar arquivo
                printf("Digite o caminho do arquivo local: ");
                gets_s(caminhoLocal, MAX_PATH);
                printf("Digite o caminho para salvar no servidor: ");
                gets_s(caminhoRemoto, MAX_PATH);
                enviarArquivo(connectSocket, caminhoLocal, caminhoRemoto);
                break;
                
            case 4: // Remover arquivo
                printf("Digite o caminho do arquivo remoto a ser removido: ");
                gets_s(caminhoRemoto, MAX_PATH);
                removerArquivo(connectSocket, caminhoRemoto);
                break;
                
            case 0: // Sair
                printf("Encerrando cliente...\n");
                break;
                
            default:
                printf("Opção inválida!\n");
                break;
        }
    }
    
    // Fecha a conexão
    closesocket(connectSocket);
    WSACleanup();
    
    return 0;
}
