#include "CommandHandler.hpp"
#include <sys/socket.h> // For send()
#include <unistd.h>     // For close()

template<typename T>
CommandHandler<T>::CommandHandler(std::shared_ptr<std::atomic<bool>> serverRunning, size_t numThreads)
    : serverRunning(serverRunning),
      // ActiveObject for parsing commands
      parserAO([this](std::shared_ptr<Message<T>> msg) {
          // Parsing logic
          std::istringstream iss(msg->rawCommand);
          iss >> msg->cmd;

          if (msg->cmd == "Newgraph") {
              int V;
              iss >> V;
              if (V <= 0) {
                  msg->error = true;
                  msg->errorMessage = "Invalid number of vertices.";
              } else {
                  auto graph = std::make_unique<Graph<T>>(V);
                  {
                      // Lock to protect access to clientGraphs
                      std::lock_guard<std::mutex> lock(graphMutex);
                      clientGraphs[msg->client_fd] = std::move(graph);
                  }
                  msg->response = "New graph created with " + std::to_string(V) + " vertices.";
              }
              // Send response to the client
              sendResponse(msg);
          }
          else if (msg->cmd == "Newedge") {
              {
                  std::lock_guard<std::mutex> lock(graphMutex);
                  if (clientGraphs.find(msg->client_fd) == clientGraphs.end()) {
                      msg->error = true;
                      msg->errorMessage = "No graph created. Use 'Newgraph' first.";
                  } else {
                      int u, v;
                      T weight;
                      iss >> u >> v >> weight;
                      auto& graph = clientGraphs[msg->client_fd];
                      int adjSize = static_cast<int>(graph->getAdjacencyList().size());
                      if (u < 0 || v < 0 || u >= adjSize || v >= adjSize) {
                          msg->error = true;
                          msg->errorMessage = "Invalid vertex indices.";
                      } else {
                          graph->addEdge(u, v, weight);
                          msg->response = "Edge added between " + std::to_string(u) + " and " + std::to_string(v) + " with weight " + std::to_string(weight) + ".";
                      }
                  }
              }
              // Send response to the client
              sendResponse(msg);
          }
          else if (msg->cmd == "MST") {
              iss >> msg->algo >> msg->pattern;
              if (msg->algo.empty() || msg->pattern.empty()) {
                  msg->error = true;
                  msg->errorMessage = "Usage: MST <algo> <design pattern>";
                  sendResponse(msg);
              } else {
                  if (msg->pattern == "Pipeline") {
                      // Send the task to calculate MST using the pipeline
                      mstAO.send(msg);
                  } else if (msg->pattern == "LF") {
                      // Process the task using Leader-Follower thread pool
                      threadPool.enqueue([this, msg]() {
                          // Calculate MST
                          {
                              std::lock_guard<std::mutex> lock(graphMutex);
                              if (clientGraphs.find(msg->client_fd) == clientGraphs.end()) {
                                  msg->error = true;
                                  msg->errorMessage = "No graph created. Use 'Newgraph' first.";
                                  sendResponse(msg);
                                  return;
                              } else {
                                  auto& graph = clientGraphs[msg->client_fd];
                                  try {
                                      auto mstSolver = MSTFactory<T>::createSolver(msg->algo);
                                      msg->mst = std::make_unique<Tree<T>>(mstSolver->solveMST(*graph));
                                  } catch (const std::exception& e) {
                                      msg->error = true;
                                      msg->errorMessage = e.what();
                                      sendResponse(msg);
                                      return;
                                  }
                              }
                          }

                          // Calculate metrics
                          if (!msg->mst) {
                              msg->error = true;
                              msg->errorMessage = "MST not available.";
                              sendResponse(msg);
                              return;
                          }
                          T totalWeight = msg->mst->getTotalWeight();
                          T longestDistance = msg->mst->findLongestDistance();
                          double averageDistance = msg->mst->findAverageDistance();
                          T shortestDistance = msg->mst->findShortestDistance();

                          // Prepare response
                          std::ostringstream oss;
                          oss << "MST successfully calculated.\n";
                          oss << "Total weight: " << totalWeight << "\n";
                          oss << "Longest distance: " << longestDistance << "\n";
                          oss << "Average distance: " << averageDistance << "\n";
                          oss << "Shortest distance: " << shortestDistance << "\n";

                          msg->response = oss.str();
                          // Send response to the client
                          sendResponse(msg);
                      });
                  } else {
                      msg->error = true;
                      msg->errorMessage = "Unknown design pattern. Use 'Pipeline' or 'LF'.";
                      sendResponse(msg);
                  }
              }
          }
          else if (msg->cmd == "exit") {
              // Handle exit command
              msg->response = "exit";
              sendResponse(msg);
          }
          else {
              // Unknown command
              msg->error = true;
              msg->errorMessage = "Unknown command.";
              sendResponse(msg);
          }
      }),
      // ActiveObject for MST calculations
      mstAO([this](std::shared_ptr<Message<T>> msg) {
          // MST calculation step
          {
              std::lock_guard<std::mutex> lock(graphMutex);
              if (clientGraphs.find(msg->client_fd) == clientGraphs.end()) {
                  msg->error = true;
                  msg->errorMessage = "No graph created. Use 'Newgraph' first.";
                  sendResponse(msg);
                  return;
              } else {
                  auto& graph = clientGraphs[msg->client_fd];
                  try {
                      auto mstSolver = MSTFactory<T>::createSolver(msg->algo);
                      msg->mst = std::make_unique<Tree<T>>(mstSolver->solveMST(*graph));
                  } catch (const std::exception& e) {
                      msg->error = true;
                      msg->errorMessage = e.what();
                      sendResponse(msg);
                      return;
                  }
              }
          }
          // Pass to the metrics calculation step
          metricsAO.send(msg);
      }),
      // ActiveObject for metrics calculations
      metricsAO([this](std::shared_ptr<Message<T>> msg) {
          // Metrics calculation step
          if (!msg->mst) {
              msg->error = true;
              msg->errorMessage = "MST not available.";
              sendResponse(msg);
              return;
          }
          T totalWeight = msg->mst->getTotalWeight();
          T longestDistance = msg->mst->findLongestDistance();
          double averageDistance = msg->mst->findAverageDistance();
          T shortestDistance = msg->mst->findShortestDistance();

          // Prepare response
          std::ostringstream oss;
          oss << "MST successfully calculated.\n";
          oss << "Total weight: " << totalWeight << "\n";
          oss << "Longest distance: " << longestDistance << "\n";
          oss << "Average distance: " << averageDistance << "\n";
          oss << "Shortest distance: " << shortestDistance << "\n";

          msg->response = oss.str();
          // Send response to the client
          sendResponse(msg);
      }),
      threadPool(numThreads)
{

}

template<typename T>
CommandHandler<T>::~CommandHandler() {
    // Ensure that all ActiveObjects are stopped and resources are freed
    parserAO.stop();
    mstAO.stop();
    metricsAO.stop();
    threadPool.stop();
}

template<typename T>
void CommandHandler<T>::handleCommand(int client_fd, const std::string& command) {
    auto msg = std::make_shared<Message<T>>();
    msg->client_fd = client_fd;
    msg->rawCommand = command;
    msg->error = false;
    // Send the message to the parser ActiveObject for processing
    parserAO.send(msg);
}

template<typename T>
void CommandHandler<T>::sendResponse(std::shared_ptr<Message<T>> msg) {
    // Send the response back to the client
    std::string fullResponse;
    if (msg->error) {
        fullResponse = "Error: " + msg->errorMessage + "\n";
    } else {
        fullResponse = msg->response + "\n";
    }
    send(msg->client_fd, fullResponse.c_str(), fullResponse.size(), 0);

    // If the command is 'exit', close the connection and stop the server
    if (msg->cmd == "exit") {
        // Set serverRunning to false
        serverRunning->store(false);
    }
}

// Explicit instantiation of the template for type double
template class CommandHandler<double>;