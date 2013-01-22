//============================================================================
// Name        : OctaneCrawler.cpp
// Author      : Berlin Brown (berlin dot brown at gmail.com)
// Version     :
// Copyright   : Copyright Berlin Brown 2012-2013
// License     : BSD
// Description : This is the simplest possible web-crawler in C++
//               Uses boost_regex and boost_algorithm
//               Simplest web-crawler in honor of Aaron Swartz
// More Info   : http://code.google.com/p/octane-crawler/
//============================================================================

#include <iostream>
#include <string>
#include <typeinfo>
#include <cstdarg>
#include <iostream>
#include <fstream>
#include <vector>

#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

// include log4cxx header files.
#include <log4cxx/logger.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/helpers/exception.h>

using namespace std;
using namespace boost;
using namespace log4cxx;
using namespace log4cxx::helpers;

const int DELAY = 6;
const int MAXRECV = 140 * 1024;
const std::string WRITE_DIR_PATH = "/home/bbrown/workspace/OctaneCrawler/octane_bot_store";

// See: http://logging.apache.org/log4cxx/index.html
LoggerPtr logger(Logger::getLogger("OctaneCrawler"));

class WebPage {
public:
    std::string hostname;
    std::string page;

    WebPage() {
        hostname = "";
        page = "";
    }

    std::string parseHttp(const std::string str) {
        const boost::regex re("(?i)http://(.*)/?(.*)");
        boost::smatch what;
        if (boost::regex_match(str, what, re)) {
            std::string hst = what[1];
            boost::algorithm::to_lower(hst);
            return hst;
        }
        return "";
    } // End of method //

    void parseHref(const std::string orig_host, const std::string str) {
        const boost::regex re("(?i)http://(.*)/(.*)");
        boost::smatch what;
        if (boost::regex_match(str, what, re)) {
            // We found a full URL, parse out the 'hostname'
            // Then parse out the page
            hostname = what[1];
            boost::algorithm::to_lower(hostname);
            page = what[2];
        } else {
            // We could not find the 'page' but we can build the hostname
            hostname = orig_host;
            page = "";
        } // End of the if - else //
    } // End of method //

    void parse(const std::string orig_host, const std::string hrf) {
        const std::string hst = parseHttp(hrf);
        if (!hst.empty()) {
            // If we have a HTTP prefix
            // We could end up with a 'hostname' and page
            parseHref(hst, hrf);
        } else {
            hostname = orig_host;
            page = hrf;
        }
        // hostname and page are constructed,
        // perform post analysis
        if (page.length() == 0) {
            page = "/";
        } // End of the if //
    } // End of the method

    void parse_link_db_line(const std::string line) {
        vector<string> link_result;
        boost::split(link_result, line, boost::is_any_of(" "));
        hostname = link_result[1];
        page = link_result[2];
    }
}; // End of the class

int connect(vector < vector <string> >* links, const std::string host, const std::string path);
void connect_page(vector < vector <string> >* links, WebPage* page, const std::string host, const std::string href);

std::string string_format(const std::string &fmt, ...) {
    int size = 255;
    std::string str;
    va_list ap;
    while (1) {
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char *) str.c_str(), size, fmt.c_str(), ap);
        va_end(ap);
        if (n > -1 && n < size) {
            str.resize(n);
            return str;
        }
        if (n > -1)
            size = n + 1;
        else
            size *= 2;
    } // End of the while //
    return str;
} // End of the function //

std::string request(std::string host, std::string path) {
    std::string request = "GET ";
    request.append(path);
    request.append(" HTTP/1.1\r\n");
    request.append("Host: ");
    request.append(host);
    request.append("\r\n");
    request.append("Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.81\r\n");
    request.append("User-Agent: Mozilla/5.0 (compatible; octanebot/1.0; http://code.google.com/p/octane-crawler/)\r\n");
    request.append("Connection: close\r\n");
    request.append("\r\n");
    return request;
} // End of the function //

std::string clean_href(const std::string host, const std::string path) {
    // Clean the href to save to file //
    std::string full_url = host;
    full_url.append("/");
    full_url.append(path);
    const boost::regex rmv_all("[^a-zA-Z0-9]");
    const std::string s2 = boost::regex_replace(full_url, rmv_all, "_");
    return s2;
}

void parse_response(vector < vector <string> >* links, const std::string recv, const std::string host, const std::string path) {
    // Parse the data //
    try {
        const boost::regex rmv_all("[\\r|\\n]");
        const std::string s2 = boost::regex_replace(recv, rmv_all, "");
        const std::string s = s2;
        // Use this regex expression, allow for mixed-case
        // Search for the anchor tag but not the '>'
        // Where (.+?) match anything
        // Common search for href "<a\\s+href\\s*=\\s*(\"([^\"]*)\")|('([^']*)')\\s*>"
        const boost::regex re("(?i)<a\\s+href\\s*=\\s*(\"([^\"]*)\")|('([^']*)')\\s*>");
        boost::cmatch matches;
        // Using token iterator with sub-matches (E.g. 2 4)
        const int subs[] = { 2, 4 };
        boost::sregex_token_iterator i(s.begin(), s.end(), re, subs);
        boost::sregex_token_iterator j;
        for (; i != j; i++) {
            // Iterate through the listed HREFs and
            // move to next request //
            const std::string href = *i;
            if (href.length() != 0) {
                WebPage* page = new WebPage();
                connect_page(links, page, host, href);
                delete page;
            } // End of the if ///
        } // End of the for //
    } catch (boost::regex_error& e) {
        cout << "Error: " << e.what() << "\n";
    } // End of the try - catch //
} // End of the function //

void parse_response2(vector < vector <string> >* links, const std::string recv, const std::string host, const std::string path) {
    try {
        const boost::regex rmv_all("[\\r|\\n]");
        const std::string s2 = boost::regex_replace(recv, rmv_all, "");
        const std::string s = s2;
        const boost::regex re("(?i)<a\\s+href\\s*=\\s*([^>]*)\\s*>([^<]*)</a>");
        boost::cmatch matches;
        const int subs[] = { 1 };
        boost::sregex_token_iterator i(s.begin(), s.end(), re, subs);
        boost::sregex_token_iterator j;
        for (; i != j; i++) {
            const std::string href = *i;
            if (href.length() != 0) {
                WebPage* page = new WebPage();
                connect_page(links, page, host, href);
                delete page;
            } // End of the if ///
        } // End of the for //
    } catch (boost::regex_error& e) {
        cout << "Error: " << e.what() << "\n";
    } // End of the try - catch //
} // End of the function //

void connect_page(vector < vector <string> >* links, WebPage* page, const std::string host, const std::string href) {
    page->parse(host, href);
    const char* hrefc = page->page.c_str();
    LOG4CXX_INFO(logger, "Adding link with : " << page->hostname << " page=" << hrefc);

    vector<string> startLinks;
    startLinks.push_back(page->hostname);
    startLinks.push_back(string_format("/%s", hrefc));
    links->push_back(startLinks);

    // sleep(DELAY);
    // Previously : connect(links, page->hostname, string_format("/%s", hrefc));
} // End of the function //

int connect(vector < vector <string> >* links, const std::string host, const std::string path) {
    const int port = 80;
    // Setup the msock
    int m_sock;
    sockaddr_in m_addr;
    memset(&m_addr, 0, sizeof(m_addr));
    m_sock = socket(AF_INET, SOCK_STREAM, 0);
    int on = 1;
    if (setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (const char*) &on, sizeof(on)) == -1) {
        return false;
    }
    struct hostent *h;
    if ((h = gethostbyname(host.c_str())) == NULL) {
        return false;
    }
    // Connect //
    const char *ip_addr = inet_ntoa(*((struct in_addr *)h->h_addr));
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = htons(port);
    int status = inet_pton(AF_INET, ip_addr, &m_addr.sin_addr);

    if (errno == EAFNOSUPPORT) {
        return false;
    }
    status = ::connect(m_sock, (sockaddr *) &m_addr, sizeof(m_addr));
    if (status == -1) {
        return false;
    }
    // HTTP/1.1 defines the "close" connection
    // the sender to signal that the connection will be closed
    // after completion of the response.
    std::string req = request(host, path);
    // End of building the request //

    status = ::send(m_sock, req.c_str(), req.size(), MSG_NOSIGNAL);
    char buf[MAXRECV];

    LOG4CXX_INFO(logger, "====== BEGIN ==");
    LOG4CXX_INFO(logger, "Full HTTP Client Request: " << req);
    LOG4CXX_INFO(logger, "====== END ==");
    LOG4CXX_INFO(logger, "");

    std::string recv = "";
    while (status != 0) {
        memset(buf, 0, MAXRECV);
        status = ::recv(m_sock, buf, MAXRECV, 0);
        recv.append(buf);
    } // End of the while //
    LOG4CXX_INFO(logger, "====== BEGIN-X ==");
    LOG4CXX_INFO(logger, "Full HTTP Client Response: " << req);
    LOG4CXX_INFO(logger, "====== END-X ==");
    LOG4CXX_INFO(logger, "");

    // Attempt to write to file //
    const std::string html_file_write = string_format("%s/%s", WRITE_DIR_PATH.c_str(), clean_href(host, path).c_str());
    LOG4CXX_INFO(logger, "Writing HTML document to file : " << html_file_write);
    ofstream outfile(html_file_write.c_str());
    outfile << recv << endl;
    outfile.close();
    parse_response(links, recv, host, path);
    parse_response2(links, recv, host, path);
    return 1;
} // End of the function //

void load_links_file() {
    LOG4CXX_INFO(logger, "$$ Reading links database : ");
    vector < vector <string> > links;
    // Read the file and crawl //
    // 100 requests at a time  //
    ifstream myfile ("./bot_db/_oct_links.db");
    string line;
    if (myfile.is_open()) {
        while (myfile.good()) {
          getline (myfile, line);
          WebPage* page = new WebPage();
          page->parse_link_db_line(line);
          delete page;
        } // End of the while //
        myfile.close();
    } // End of the if //
} // End of the function //

int main() {
    LOG4CXX_INFO(logger, "$$ Launching OctaneCrawler application : vers=" << "1.0");
    vector < vector <string> > links;
    LOG4CXX_INFO(logger, "$$ Initiating process ...")
    const std::string host = "theinfo.org";
    const std::string page = "/";
    connect(&links, host, page);
    LOG4CXX_INFO(logger, "$$ Size of link set : " << links.size());

    // Loop through all the links and write to file, lisp format //
    ofstream outfile("./bot_db/_oct_links.db");
    for (unsigned int i = 0; i < links.size(); i++) {
        vector <string> v = links.at(i);
        const string h = v.at(0);
        const string p = v.at(1);
        outfile << "(set-link " << h << " " << " " << p << " 0)" << endl;
    } // End of the for //
    outfile.close();
    sleep(DELAY);
    load_links_file();

    LOG4CXX_INFO(logger, "$$ Done : Exiting application.")
    return 0;
} // End of the function //
