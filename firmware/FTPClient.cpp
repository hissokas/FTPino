/*****************************************************************************
 *
 * This file is part of FTPino.
 *
 * FTPino is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FTPino is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FTPino.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/
#include "Config.h"
#ifdef CLIENT_ACTIVE
#include "FTPClient.h"

char FTPClient::readServerResponse(char *outBuf) {
  auto result = 1;
  char thischar;
  memset(outBuf, 0, sizeof(outBuf));
  uint8_t outCount;
  if (!client.connected()) {
    Particle.publish("No client connected!");
    result = 0;
  }

  while (!client.available())
    Particle.process(); // run main loop while waiting for client
  outCount = 0;
  unsigned long maxInterations = 10000;
  do {
    while (client.available()) {
      thischar = client.read();

      if (outCount < 127) {
        outBuf[outCount] = thischar;
        outCount++;
        outBuf[outCount] = 0;
      }
    }

  } while (
      (outCount < 3) &&
      (--maxInterations >
       0)); // at least 3 characters represending a FTF code have been received
  client.flush();

  return result; // ok
}

String FTPClient::send(String &stringToWrite, String &remoteFile,
                       TE_FTPClient_WriteMode writeMode) {
  char outBuf[128];
  client.connect(server, 21);
  int maxInterations = 10000;
  while ((client.connected()) && (--maxInterations > 0))
    ;

  if (!client.connected())
    return "Error, cannot connect to FTP.";

  if (!readServerResponse(outBuf))
    return "Error when connecting to FTP Server.";

  client.println("CLIENT FTPino");
  if (!readServerResponse(outBuf))
    return "Error when sending USER (credential username).";
  client.println("USER " + username);
  if (!readServerResponse(outBuf))
    return "Error when sending USER (credential username).";
  client.println("PASS " + password);
  if (!readServerResponse(outBuf))
    return "Error when sening PASS (credential password).";
  client.println("SYST");
  if (!readServerResponse(outBuf))
    return "Error when sending SYST.";
  client.println("TYPE I");
  if (!readServerResponse(outBuf))
    return "Error when sending Type I.";
  client.println("PASV");
  if (!readServerResponse(outBuf))
    return "Error when sending PASV.";
  char *tStr = strtok(
      outBuf,
      "(,"); // tokenizing response of server, getting ports for data transfer

  int array_pasv[6];
  for (int i = 0; i < 6; i++) {
    tStr = strtok(NULL, "(,");
    array_pasv[i] = atoi(tStr);
    if (tStr == NULL) {
      return "Error when tokenizing server response for passive data ports.";
    }
  }

  if (!dclient.connect(
          server,
          (array_pasv[4] << 8) |
              (array_pasv[5] & 255))) { // opening new datastream with the ports
                                        // from the tokenized server response
    client.stop();
    return "Connection Error when creating second FTP Socket.";
  }

  String writeModeCommand;
  switch (writeMode) {
  case TE_FTPClient_WriteMode_Append:
    writeModeCommand = "APPE";
    break;
  case TE_FTPClient_WriteMode_Overwrite:
    writeModeCommand = "STOR";
    break;
  TE_FTPClient_WriteMode_Unknown:
  default:
    return "Error, unknown write mode for passive data socket.";
    break;
  }

  client.println(writeModeCommand + " " + remoteFile);
  if (!readServerResponse(outBuf)) {
    dclient.stop();
    return "Error when sending " + writeModeCommand + ".";
  }

  char clientBuf[64];
  uint32_t clientCount = 0, posInOutString = 0;
  do {
    clientBuf[clientCount++] = stringToWrite[posInOutString++];

    if (clientCount >= sizeof(clientBuf)) {
      dclient.write(reinterpret_cast<const uint8_t *>(clientBuf),
                    sizeof(clientBuf));
      clientCount = 0;

      delay(100); // give the server a chance to interpret the data. Only
                  // necessary if server is FTPino
    }
  } while (posInOutString < stringToWrite.length());

  if (clientCount > 0)
    dclient.write(reinterpret_cast<const uint8_t *>(clientBuf), clientCount);
  delay(100); // finish off wriring what's left in the buffer

  dclient.stop();
  if (!readServerResponse(outBuf))
    return "Error when stopping client.";
  client.println("QUIT");
  if (!readServerResponse(outBuf))
    return "Error when sending QUIT to server.";
  client.stop();

  return "FTP Success.";
}

void FTPClient::onFail() {

  client.println("QUIT");
  client.stop();
}
#endif // CLIENT_ACTIVE
