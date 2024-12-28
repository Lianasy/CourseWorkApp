// server.cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <winsock2.h>
#include <thread>
#include <mutex>
#include <set>
#include <algorithm>
#include <queue>
#include <condition_variable>


#pragma comment(lib, "ws2_32.lib")
#define PORT 8080
#define BUFFER_SIZE 4096

/**
* @brief Represents a movie with various attributes.
*
* The `Movie` struct encapsulates information about a single movie, including:
* - ID: A unique identifier for the movie.
* - Title: The name of the movie.
* - Overview: A brief description of the movie's plot or storyline.
* - Language: The primary language in which the movie is available.
* - Genres: A list of genres that categorize the movie.
* - Year: The release year of the movie.
* - Rating: The movie's average rating on a scale of 0 to 10.
* - Poster URL: A link to the movie's poster image.
*/
struct Movie {
    int id;
    std::string title, overview, language;
    std::vector<std::string> genres;
    int year;
    float rating;
    std::string posterUrl;
};


/**
 * @brief A thread-safe inverted index for efficient movie search.
 *
 * This class maps keywords and attributes (e.g., genres, language, title) to
 * sets of movie IDs, allowing fast and flexible search operations.
 */
class InvertedIndex {
    std::unordered_map<std::string, std::unordered_set<int>> index;
    std::mutex mtx;

public:
    /**
     * @brief Converts a string to lowercase.
     *
     * @param str The input string to convert.
     * @return A lowercase version of the input string.
     */
    static std::string toLower(const std::string &str) {
        std::string lowerStr = str;
        std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
        return lowerStr;
    }

    /**
     * @brief Cleans a word by removing punctuation and leading/trailing spaces.
     *
     * @param word The input word to clean.
     * @return A cleaned version of the word.
     */
    static std::string cleanWord(const std::string &word) {
        std::string clean = word;
        clean.erase(0, clean.find_first_not_of(" \".,:;!?()[]{}<>"));
        if (!clean.empty()) {
            clean.erase(clean.find_last_not_of(" \".,:;!?()[]{}<>") + 1);
        }
        return clean;
    }

    /**
     * @brief Adds a movie ID to the index under a specific keyword.
     *
     * @param key The keyword associated with the movie.
     * @param movieId The ID of the movie to add.
     */
    void addToIndex(const std::string &key, int movieId) {
        std::lock_guard<std::mutex> lock(mtx);
        index[key].insert(movieId);
    }

    /**
     * @brief Indexes a movie by its attributes.
     *
     * Creates index entries for genres, year, language, title, and overview.
     *
     * @param id The ID of the movie.
     * @param movie The movie object to index.
     */
    void addMovie(int id, const Movie &movie) {
        for (const auto& genre : movie.genres) {
            std::istringstream stream(genre);
            std::string word;
            while (stream >> word) { // Разбиваем жанр на слова
                addToIndex("genre_" + toLower(word), id);
            }
        }
        addToIndex("year_" + std::to_string(movie.year), id);
        addToIndex("language_" + toLower(movie.language), id);
        for (int i = static_cast<int>(movie.rating); i <= 10; ++i) {
            addToIndex("rating_" + std::to_string(i), id);
        }

        auto processText = [&](const std::string &text, const std::string &category) {
            std::istringstream stream(text);
            std::string word;
            while (stream >> word) {
                word = toLower(cleanWord(word));
                if (!word.empty()) {
                    addToIndex(category + word, id);
                }
            }
        };

        processText(movie.title, "title_");
        processText(movie.overview, "overview_");
    }

    /**
     * @brief Searches the index by category and value.
     *
     * @param category The category to search (e.g., "genre").
     * @param value The specific value within the category (e.g., "Action").
     * @return A set of movie IDs matching the query.
     */
    std::unordered_set<int> searchByCategory(const std::string &category, const std::string &value) {
        std::lock_guard<std::mutex> lock(mtx);
        std::string key = category + "_" + toLower(value);
        if (index.find(key) != index.end()) {
            return index[key];
        }
        return {};
    }

    /**
     * @brief Searches the index using multiple keywords.
     *
     * Combines results across categories (e.g., title, overview, genres) for all keywords.
     *
     * @param keys A vector of keywords to search for.
     * @return A set of movie IDs matching all keywords.
     */
    std::unordered_set<int> searchByKeywords(const std::vector<std::string> &keys) {
        std::lock_guard<std::mutex> lock(mtx);
        std::unordered_set<int> currentResults;
        bool isFirst = true;

        for (const auto &key: keys) {
            std::string cleanedKey = toLower(cleanWord(key));
            std::unordered_set<int> keyResults;

            for (const std::string &category: {"title_", "overview_", "genre_", "language_", "year_", "rating_"}) {
                std::string fullKey = category + cleanedKey;
                if (index.find(fullKey) != index.end()) {
                    keyResults.insert(index[fullKey].begin(), index[fullKey].end());
                }
            }

            if (isFirst) {
                currentResults = keyResults;
                isFirst = false;
            } else {
                std::unordered_set<int> intersection;
                for (int id: currentResults) {
                    if (keyResults.find(id) != keyResults.end()) {
                        intersection.insert(id);
                    }
                }
                currentResults = intersection;
            }

            if (currentResults.empty()) {
                break;
            }
        }

        return currentResults;
    }

    std::unordered_set<int>

    /**
     * @brief Finds the intersection of two sets of movie IDs.
     *
     * @param baseResults The base set of results.
     * @param additionalResults The additional set of results.
     * @return A set containing the intersection of both input sets.
     */
    intersectResults(const std::unordered_set<int> &baseResults, const std::unordered_set<int> &additionalResults) {
        std::unordered_set<int> intersection;
        for (int id: baseResults) {
            if (additionalResults.find(id) != additionalResults.end()) {
                intersection.insert(id);
            }
        }
        return intersection;
    }

    /**
     * @brief Provides access to the raw index data.
     *
     * @return A constant reference to the index.
     */
    const std::unordered_map<std::string, std::unordered_set<int>> &getIndexData() const {
        return index;
    }

    /**
     * @brief Clears all data from the index.
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mtx);
        index.clear();
    }

    /**
     * @brief Locks the index and provides thread-safe access.
     *
     * @return A lock guard for the index mutex.
     */
    std::lock_guard<std::mutex> lockIndex() {
        return std::lock_guard<std::mutex>(mtx);
    }
};

InvertedIndex invertedIndex;
std::vector<Movie> movies;
std::unordered_set<std::string> genres, languages;
std::set<int> years;
std::set<float> ratings;

/**
 * @brief Loads movie data from a file, processes it in parallel, and updates global data structures.
 *
 * This function divides the input file into chunks, processes each chunk using multiple threads,
 * and combines the results into global variables for movies, genres, languages, years, and ratings.
 * It also builds an inverted index for efficient searching.
 *
 * @param filePath Path to the input CSV file containing movie data.
 * @param numThreads Number of threads to use for parallel processing.
 */
void loadMovies(const std::string &filePath, size_t numThreads) {
    // Open the file in binary mode to determine its size.
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file!" << std::endl;
        return;
    }

    size_t fileSize = file.tellg(); // Get the size of the file.
    file.close();

    // Calculate chunk size for each thread.
    size_t chunkSize = fileSize / numThreads;

    // Temporary structures to store thread-specific results.
    std::vector<std::vector<Movie>> threadMovies(numThreads);
    std::vector<std::unordered_set<std::string>> threadGenres(numThreads);
    std::vector<std::unordered_set<std::string>> threadLanguages(numThreads);
    std::vector<std::set<int>> threadYears(numThreads);
    std::vector<std::set<float>> threadRatings(numThreads);

    // Lambda function for processing a chunk of the file.
    auto processChunk = [&](size_t threadId, size_t startPos, size_t endPos) {
        std::ifstream inFile(filePath);
        inFile.seekg(startPos); // Move to the starting position of the chunk.

        std::string line;
        if (startPos != 0) {
            std::getline(inFile, line); // Skip partial line at the beginning of the chunk.
        }

        // Read and process each line within the chunk's range.
        while (std::getline(inFile, line) && inFile.tellg() <= static_cast<std::streampos>(endPos)) {
            std::vector<std::string> fields;
            std::string current;
            bool inQuotes = false;

            // Parse the CSV line, handling quoted fields.
            for (char c: line) {
                if (c == '"') {
                    inQuotes = !inQuotes;
                } else if (c == ',' && !inQuotes) {
                    fields.push_back(current);
                    current.clear();
                } else {
                    current += c;
                }
            }
            fields.push_back(current); // Add the last field.

            if (fields.size() < 9) continue; // Skip malformed lines.

            Movie movie{static_cast<int>(threadMovies[threadId].size())}; // Create a new movie.
            try {
                // Populate the movie fields.
                movie.year = fields[0].empty() ? 0 : stoi(fields[0].substr(0, 4));
                movie.title = fields[1];
                movie.overview = fields[2];
                movie.rating = fields[5].empty() ? 0.0f : stof(fields[5]);
                movie.language = fields[6];
                movie.posterUrl = fields[8];

                // Parse and clean genres.
                std::istringstream genreStream(fields[7]);
                std::string genre;
                while (getline(genreStream, genre, ',')) {
                    genre.erase(0, genre.find_first_not_of(" \""));
                    genre.erase(genre.find_last_not_of(" \"") + 1);
                    if (!genre.empty()) {
                        movie.genres.push_back(genre);
                        threadGenres[threadId].insert(genre);
                    }
                }
            } catch (...) {
                continue; // Skip malformed lines.
            }

            // Validate and add the movie to thread-specific results.
            if (!movie.title.empty() && !movie.overview.empty() && !movie.genres.empty() &&
                movie.year != 0 && movie.rating > 0.0f) {
                threadMovies[threadId].push_back(movie);
                threadYears[threadId].insert(movie.year);
                threadRatings[threadId].insert(movie.rating);
                threadLanguages[threadId].insert(movie.language);
            }
        }
    };

    // Launch threads to process chunks.
    std::vector<std::thread> threads;
    for (size_t i = 0; i < numThreads; ++i) {
        size_t startPos = i * chunkSize;
        size_t endPos = (i == numThreads - 1) ? fileSize : startPos + chunkSize;
        threads.emplace_back(processChunk, i, startPos, endPos);
    }

    for (auto &thread : threads) {
        thread.join(); // Wait for all threads to finish.
    }

    // Merge results from all threads into global variables.
    movies.clear();
    genres.clear();
    languages.clear();
    years.clear();
    ratings.clear();

    for (size_t i = 0; i < numThreads; ++i) {
        movies.insert(movies.end(), threadMovies[i].begin(), threadMovies[i].end());
        genres.insert(threadGenres[i].begin(), threadGenres[i].end());
        languages.insert(threadLanguages[i].begin(), threadLanguages[i].end());
        years.insert(threadYears[i].begin(), threadYears[i].end());
        ratings.insert(threadRatings[i].begin(), threadRatings[i].end());
    }

    // Rebuild the inverted index using the loaded movies.
    invertedIndex.clear();
    for (size_t i = 0; i < movies.size(); ++i) {
        invertedIndex.addMovie(i, movies[i]);
    }

    std::cout << "Movies loaded and indexed successfully with " << numThreads << " threads." << std::endl;
}

/**
 * @brief Handles client requests and sends appropriate responses.
 *
 * This function processes incoming HTTP requests from clients, including GET and POST requests.
 * - **GET**: Responds with an HTML form for movie search.
 * - **POST**: Processes search parameters, queries the inverted index, and returns search results.
 * - **SEARCH**: (Custom command) Performs a keyword-based search and returns results in plain text.
 *
 * @param clientSocket The socket connected to the client.
 */
void handleClient(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE] = {0};   // Buffer to store the client's request.
    int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);  // Read the request.
    if (bytesReceived <= 0) {
        closesocket(clientSocket); // Close the connection if no data is received.
        return;
    }

    std::string request(buffer); // Close the connection if no data is received.

    // Handle GET requests: send an HTML form for movie search.
    if (request.find("GET") != std::string::npos) {
        std::ostringstream response;

        // HTTP headers and opening HTML structure.
        response << "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
        response << R"(
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Movie Search</title>
        <style>
            body {
                font-family: Arial, sans-serif;
                background-color: #f5f5f5;
                display: flex;
                justify-content: center;
                align-items: center;
                min-height: 100vh;
                margin: 0;
            }
            .container {
                background: #fff;
                padding: 20px;
                border-radius: 8px;
                box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
                width: 400px;
            }
            h1 {
                text-align: center;
                margin-bottom: 20px;
            }
            label {
                font-weight: bold;
                display: block;
                margin-top: 10px;
            }
            select, input[type="text"], button {
                width: 100%;
                padding: 10px;
                margin-top: 5px;
                margin-bottom: 15px;
                border: 1px solid #ccc;
                border-radius: 5px;
            }
            button {
                background-color: #007bff;
                color: #fff;
                font-weight: bold;
                border: none;
                cursor: pointer;
            }
            button:hover {
                background-color: #0056b3;
            }
        </style>
    </head>
    <body>
        <div class="container">
            <h1>Movie Search</h1>
            <form method="POST">
    )";

        // Dropdown for genres.
        response << "<label for='genre'>Genre:</label>";
        response << "<select name='genre' id='genre'>";
        response << "<option value=''>Any</option>";
        for (const auto &genre: genres) {
            response << "<option value='" << genre << "'>" << genre << "</option>";
        }
        response << "</select>";

        // Dropdown for years.
        response << "<label for='year'>Year:</label>";
        response << "<select name='year' id='year'>";
        response << "<option value=''>Any</option>";
        for (const auto &year: years) {
            response << "<option value='" << year << "'>" << year << "</option>";
        }
        response << "</select>";

        // Dropdown for languages.
        response << "<label for='language'>Language:</label>";
        response << "<select name='language' id='language'>";
        response << "<option value=''>Any</option>";
        for (const auto &lang: languages) {
            response << "<option value='" << lang << "'>" << lang << "</option>";
        }
        response << "</select>";

        // Input field for keywords.
        response << "<label for='keywords'>Keywords:</label>";
        response
                << "<input type='text' name='keywords' id='keywords' placeholder='Enter your search keywords separated by a space'>";

        // Submit button and closing HTML.
        response << "<button type='submit'>Search</button>";
        response << R"(
            </form>
        </div>
    </body>
    </html>
    )";

        // Send the response back to the client.
        send(clientSocket, response.str().c_str(), response.str().length(), 0);
    }

    // Handle POST requests: process form data and return search results.
    if (request.find("POST") != std::string::npos) {
        // Extract the request body containing form data.
        std::string body = request.substr(request.find("\r\n\r\n") + 4);
        std::unordered_map<std::string, std::string> params;
        std::istringstream ss(body);
        std::string pair;
        while (std::getline(ss, pair, '&')) {
            size_t eqPos = pair.find('=');
            params[pair.substr(0, eqPos)] = pair.substr(eqPos + 1);
        }

        // Search logic based on form parameters.
        std::unordered_set<int> results;
        if (!params["genre"].empty()) {
            std::string genreInput = params["genre"];
            std::replace(genreInput.begin(), genreInput.end(), '+', ' '); // Заменяем '+' на пробел

            std::istringstream genreStream(genreInput);
            std::string word;
            std::unordered_set<int> genreResults;

            while (genreStream >> word) { // Разбиваем на отдельные слова
                std::unordered_set<int> wordResults = invertedIndex.searchByCategory("genre", word);
                if (genreResults.empty()) {
                    genreResults = wordResults;
                } else {
                    genreResults = invertedIndex.intersectResults(genreResults, wordResults);
                }
            }

            if (results.empty()) {
                results = genreResults;
            } else {
                results = invertedIndex.intersectResults(results, genreResults);
            }
        }
        if (!params["year"].empty()) {
            std::unordered_set<int> yearResults = invertedIndex.searchByCategory("year", params["year"]);
            if (results.empty()) {
                results = yearResults;
            } else {
                results = invertedIndex.intersectResults(results, yearResults);
            }
        }

        if (!params["language"].empty()) {
            std::unordered_set<int> languageResults = invertedIndex.searchByCategory("language", params["language"]);
            if (results.empty()) {
                results = languageResults;
            } else {
                results = invertedIndex.intersectResults(results, languageResults);
            }
        }
        if (!params["keywords"].empty()) {
            std::istringstream kwStream(params["keywords"]);
            std::string keyword;
            std::vector<std::string> keywords;
            while (std::getline(kwStream, keyword, '+')) {
                keyword = InvertedIndex::toLower(InvertedIndex::cleanWord(keyword));
                if (!keyword.empty()) {
                    keywords.push_back(keyword);
                }
            }

            for (const auto &word: keywords) {
                std::unordered_set<int> wordResults;
                for (const std::string &category: {"title_", "overview_", "genre_", "language_", "year_", "rating_"}) {
                    std::string fullKey = category + word;
                    if (invertedIndex.getIndexData().find(fullKey) != invertedIndex.getIndexData().end()) {
                        wordResults.insert(invertedIndex.getIndexData().at(fullKey).begin(),
                                           invertedIndex.getIndexData().at(fullKey).end());
                    }
                }
                if (results.empty()) {
                    results = wordResults;
                } else {
                    results = invertedIndex.intersectResults(results, wordResults);
                }
            }
        }

        std::vector<int> sortedResults(results.begin(), results.end());
        // Sort results if specified.
        if (params["sort"] == "rating_asc") {
            std::sort(sortedResults.begin(), sortedResults.end(), [&](int a, int b) {
                return movies[a].rating < movies[b].rating;
            });
        } else if (params["sort"] == "rating_desc") {
            std::sort(sortedResults.begin(), sortedResults.end(), [&](int a, int b) {
                return movies[a].rating > movies[b].rating;
            });
        }


        // Build the HTML response with search results.
        std::ostringstream response;
        response << "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
        response << R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Movie Search Results</title>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Roboto:wght@400;500;700&display=swap');

        body {
            font-family: 'Roboto', sans-serif;
            background-color: #f5f5f5;
            margin: 0;
            padding: 0;
        }
        .container {
            max-width: 1000px;
            margin: 20px auto;
            background: #fff;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
        }
        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 20px;
        }
        .header h1 {
            margin: 0;
            font-size: 24px;
            color: #333;
        }
        .sort-buttons {
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .sort-buttons .sort-label {
            font-size: 16px;
            font-weight: 500;
            color: #333;
        }
        .arrow-button {
            background-color: #007bff;
            color: #fff;
            border: none;
            border-radius: 4px;
            padding: 5px 10px;
            font-size: 16px;
            cursor: pointer;
            line-height: 1;
        }
        .arrow-button:hover {
            background-color: #0056b3;
        }
        .movie-list {
            list-style: none;
            padding: 0;
        }
        .movie-item {
            display: flex;
            align-items: flex-start;
            padding: 15px;
            border-bottom: 1px solid #ddd;
        }
        .movie-item:last-child {
            border-bottom: none;
        }
        .poster {
            flex: 0 0 120px;
            margin-right: 15px;
        }
        .poster img {
            width: 120px;
            height: auto;
            border-radius: 4px;
        }
        .details {
            flex: 1;
        }
        .title {
            font-size: 18px;
            font-weight: bold;
            margin: 0;
            color: #007bff;
        }
        .title:hover {
            text-decoration: underline;
            cursor: pointer;
        }
        .info {
            font-size: 14px;
            color: #555;
            margin-top: 5px;
        }
        .rating {
            color: #f39c12;
            font-weight: bold;
        }
        .genres {
            font-size: 14px;
            color: #777;
            margin-top: 5px;
        }
        .overview {
            margin-top: 10px;
            font-size: 14px;
            color: #333;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Movie Search Results</h1>
            <div class="sort-buttons">
<span class="sort-label">Sort by Rating</span>
    <form method="POST" style="display: inline;">
        <input type="hidden" name="genre" value=")" << params["genre"] << R"(">
        <input type="hidden" name="year" value=")" << params["year"] << R"(">
        <input type="hidden" name="language" value=")" << params["language"] << R"(">
        <input type="hidden" name="keywords" value=")" << params["keywords"] << R"(">
        <input type="hidden" name="sort" value="rating_asc">
        <button type="submit" class="arrow-button">&#9650;</button>
    </form>
    <form method="POST" style="display: inline;">
        <input type="hidden" name="genre" value=")" << params["genre"] << R"(">
        <input type="hidden" name="year" value=")" << params["year"] << R"(">
        <input type="hidden" name="language" value=")" << params["language"] << R"(">
        <input type="hidden" name="keywords" value=")" << params["keywords"] << R"(">
        <input type="hidden" name="sort" value="rating_desc">
        <button type="submit" class="arrow-button">&#9660;</button>
    </form>
            </div>
        </div>
        <ul class="movie-list">
)";
        if (results.empty()) {
            response << "No movies match your search criteria.\n";
        } else {
            for (const auto &id: sortedResults) {
                response << R"(<li class="movie-item">)"
                         << "<div class='poster'><img src='" << movies[id].posterUrl << "' alt='Movie Poster'></div>"
                         << "<div class='details'>"
                         << "<p class='title'>" << movies[id].title << "</p>"
                         << "<p class='info'>" << movies[id].year << " | <span class='rating'>" << movies[id].rating
                         << "</span> | "
                         << "<span class='language'>" << movies[id].language << "</span></p>"
                         << "<p class='genres'>Genres: ";
                for (const auto &genre: movies[id].genres) {
                    response << genre << ", ";
                }
                response.seekp(-2, std::ios_base::cur); // Убираем последнюю запятую
                response << "</p>"
                         << "<p class='overview'>" << movies[id].overview << "</p>"
                         << "</div></li>";
            }
        }

        response << R"(
        </ul>
    </div>
</body>
</html>
)";
        // Send the response.
        send(clientSocket, response.str().c_str(), response.str().length(), 0);
    }

    if (request.find("SEARCH") == 0) {  // Check if the request is a "SEARCH" command.
        std::string keywords = request.substr(7); // Extract the keyword part after "SEARCH".
        std::istringstream kwStream(keywords);
        std::string keyword;
        std::vector<std::string> keywordsVec;

        // Process the keywords: clean, convert to lowercase, and store them.
        while (kwStream >> keyword) {
            keyword = InvertedIndex::toLower(InvertedIndex::cleanWord(keyword));
            if (!keyword.empty()) {
                keywordsVec.push_back(keyword); // Add the cleaned keyword to the vector.
            }
        }

        // Perform a keyword search using the inverted index.
        std::unordered_set<int> results = invertedIndex.searchByKeywords(keywordsVec);

        // Build the response.
        std::ostringstream response;
        if (results.empty()) {
            response << "No movies found\n"; // Inform the client if no matches are found.
        } else {
            // Iterate through the results and include movie details in the response.
            for (int id: results) {
                response << "Title: " << movies[id].title << "\n"
                         << "Year: " << movies[id].year << "\n"
                         << "Rating: " << movies[id].rating << "\n"
                         << "Language: " << movies[id].language << "\n"
                         << "Genres: ";
                for (const auto &genre: movies[id].genres) {
                    response << genre << " "; // Append each genre to the response.
                }
                response << "\nOverview: " << movies[id].overview << "\n\n";
            }
        }
        // Send the response back to the client.
        send(clientSocket, response.str().c_str(), response.str().length(), 0);
        // Close the client socket to end the connection.
        closesocket(clientSocket); // Exit after handling the "SEARCH" command.
        return;
    }


    closesocket(clientSocket);
}

/**
 * @brief A thread pool for managing client connections in a multithreaded server.
 *
 * The `ThreadPool` class creates a fixed number of worker threads to handle incoming
 * tasks (e.g., client connections). Tasks are added to a thread-safe queue and processed
 * by available threads. The pool ensures efficient resource utilization and scalability.
 */
class ThreadPool {
    std::vector<std::thread> workers; ///< Vector of worker threads.
    std::queue<SOCKET> tasks; ///< Queue of tasks (client sockets) to be processed.
    std::mutex queueMutex; ///< Mutex for synchronizing access to the task queue.
    std::condition_variable cv;  ///< Condition variable to notify threads of new tasks.
    bool stop; ///< Flag to signal threads to stop processing.

public:
    /**
     * @brief Constructs the thread pool and initializes worker threads.
     *
     * @param numThreads The number of worker threads to create.
     */
    ThreadPool(size_t numThreads) : stop(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this]() {
                while (true) {
                    SOCKET clientSocket;
                    {
                        // Lock the task queue and wait for a task or stop signal.
                        std::unique_lock<std::mutex> lock(queueMutex);
                        cv.wait(lock, [this]() { return stop || !tasks.empty(); });
                        if (stop && tasks.empty()) return; // Exit the thread if stopping.
                        clientSocket = tasks.front(); // Retrieve the next task.
                        tasks.pop(); // Remove the task from the queue.
                    }
                    handleClient(clientSocket); // Process the client request.
                }
            });
        }
    }
    /**
        * @brief Adds a new task (client socket) to the thread pool's queue.
        *
        * @param clientSocket The socket representing the client connection.
        */
    void enqueue(SOCKET clientSocket) {
        {
            std::lock_guard<std::mutex> lock(queueMutex); // Lock the queue for safe access.
            tasks.push(clientSocket); // Add the client socket to the task queue.
        }
        cv.notify_one(); // Notify one worker thread about the new task.
    }

    /**
         * @brief Destroys the thread pool and stops all worker threads.
         *
         * This destructor sets the stop flag, notifies all threads, and waits for their
         * completion before cleaning up.
         */
    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(queueMutex); // Lock the queue to set the stop flag.
            stop = true;  // Signal threads to stop processing.
        }
        cv.notify_all(); // Wake up all threads to allow them to exit.
        for (std::thread &worker: workers) {
            worker.join(); // Wait for each thread to finish.
        }
    }
};

/**
 * @brief Periodically updates the inverted index by reloading movie data from a file.
 *
 * This function runs a background thread that periodically invokes the `loadMovies` function
 * to refresh the movie data and rebuild the inverted index. It operates at a fixed interval
 * specified in minutes.
 *
 * @param filePath The path to the movie data file (CSV format).
 * @param intervalMinutes The time interval (in minutes) between updates.
 */

void updateIndexPeriodically(const std::string &filePath, int intervalMinutes) {
    // Start a detached thread for periodic updates.
    std::thread([filePath, intervalMinutes]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::minutes(intervalMinutes)); // Wait for the specified interval.
            // Reload the movie data and rebuild the index with 8 threads.
            loadMovies(filePath, 8);
        }
    }).detach(); // Detach the thread to run independently.
}

/**
 * @brief Logs the performance data of a computation or operation.
 *
 * This function writes the number of threads used and the execution time of an operation
 * to a specified log file. The data is appended to the file in a CSV format for further analysis.
 *
 * @param filePath The path to the log file where performance data will be stored.
 * @param numThreads The number of threads used during the operation.
 * @param executionTime The execution time of the operation in seconds.
 */
void logPerformanceData(const std::string &filePath, size_t numThreads, double executionTime) {
    // Open the log file in append mode to preserve existing data.
    std::ofstream logFile(filePath, std::ios::app);
    // Check if the file was successfully opened.
    if (logFile.is_open()) {
        // Write performance data as a CSV line.
        logFile << numThreads << "," << executionTime << std::endl;
        logFile.close();
    } else {
        // Log an error message if the file could not be opened.
        std::cerr << "Error: Unable to open log file!" << std::endl;
    }
}

/**
 * @brief Entry point of the server application.
 *
 * This function initializes the server, loads movie data, starts periodic index updates,
 * and processes incoming client requests using a thread pool.
 *
 * Key Steps:
 * 1. Load and index movie data in parallel.
 * 2. Log performance metrics.
 * 3. Periodically update the inverted index.
 * 4. Set up and run a multithreaded server to handle client requests.
 *
 * @return 0 on successful execution.
 */
int main() {
    std::string path = "C:/My things/Uni 7 term/course work/CourseWorkApp/9000plus.csv"; // Movie data file path.
    std::string logFilePath = "performance_log.csv"; // Log file path for performance data.
    size_t numThreads = 8; // Number of threads for parallel processing.

    // Measure and log the performance of the initial movie data load.
    auto start = std::chrono::high_resolution_clock::now();
    loadMovies(path, numThreads);
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end - start;

    logPerformanceData(logFilePath, numThreads, elapsed.count());

    // Start periodic updates of the movie index every minute.
    updateIndexPeriodically(path, 1);

    // Initialize Winsock and create a server socket.
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    // Configure the server address and bind the socket.
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (sockaddr *) &serverAddr, sizeof(serverAddr));
    listen(serverSocket, SOMAXCONN);

    // Create a thread pool for handling client connections.
    ThreadPool pool(std::thread::hardware_concurrency());

    // Start accepting and processing client requests.
    std::cout << "Server is running on port " << PORT << std::endl;
    std::cout << "Open this link in your browser: http://127.0.0.1:" << PORT << std::endl;

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Error: Unable to accept connection!" << std::endl;
            continue;
        }
        pool.enqueue(clientSocket); // Delegate the request to the thread pool.
    }

    // Close the server socket and shutdown Winsock.
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}