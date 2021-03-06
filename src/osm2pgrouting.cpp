/***************************************************************************
 *   Copyright (C) 2008 by Daniel Wendt               					   *
 *   gentoo.murray@gmail.com   											   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include <unistd.h>
#include <string>
#include <iostream>
#include <ctime>
#include <chrono>
#include "./ConfigurationParserCallback.h"
#include "./OSMDocument.h"
#include "./OSMDocumentParserCallback.h"
#include "./Export2DB.h"
#include "./prog_options.h"



int main(int argc, char* argv[]) {
    //  Start Timers
    clock_t begin = clock();
    std::time_t start_t = std::time(NULL);
    std::chrono::steady_clock::time_point begin_elapsed = std::chrono::steady_clock::now();
    try {

        po::options_description od_desc("Allowed options");
        get_option_description(od_desc);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).
            options(od_desc).run(), vm);

        if (vm.count("help")) {
            std::cout << od_desc << "\n";
            return 0;
        }

        if (vm.count("version")) {
            std::cout << "This is osm2pgrouting Version 2.1\n";
            return 0;
        }

        try {
            notify(vm);
        }

        catch(exception &ex) {
            std::cout << ex.what() << "\n";
            std::cout << od_desc << "\n";
            return 0;
        }

        std::cout << "Execution starts at: " << std::ctime(&start_t) << "\n";
        process_command_line(vm);

        auto dataFile(vm["file"].as<string>());
        auto confFile(vm["conf"].as<string>());
        auto skipnodes(!vm.count("addnodes"));
        auto clean(vm.count("clean"));


        std::cout << "Connecting to the database"  << endl;
            Export2DB dbConnection(vm);
            if (dbConnection.connect() == 1)
                return 1;
            if (!dbConnection.has_postGIS()) {
                std::cout << "ERROR: postGIS not found\n";
                return 1;
            }


        std::cout << "Opening configuration file: " << confFile.c_str() << endl;
            Configuration* config = new Configuration();
            ConfigurationParserCallback cCallback(*config);


        std::cout << "    Parsing configuration\n" << endl;
            xml::XMLParser parser;
            int ret = parser.Parse(cCallback, confFile.c_str());
            if (ret != 0) {
                cout << "Failed to open / parse config file " << confFile.c_str() << endl;
                return 1;
            }


        std::cout << "Opening data file: " << dataFile.c_str() << endl;
            OSMDocument *document = new OSMDocument(*config);
            OSMDocumentParserCallback callback(*document);

        std::cout << "    Parsing data\n" << endl;
        ret = parser.Parse(callback, dataFile.c_str());
        if (ret != 0) {
            cerr << "Failed to open / parse data file " << dataFile.c_str() << endl;
            return 1;
        }

        std::cout << "Spliting ways\n" << endl;
            document->SplitWays();

        //############# Export2DB
        { 
        if (clean) {
            std::cout << "Dropping tables..." << endl;
            dbConnection.dropTables();
        }

        std::cout << "Creating tables..." << endl;
            dbConnection.createTables();

        std::cout << "Adding auxiliary tables to database..." << endl;
            if (!skipnodes) {
                dbConnection.exportNodes(document->m_Nodes);
            }
            dbConnection.exportTypes(config->m_Types);
            dbConnection.exportClasses(config->m_Types);
            //dbConnection.exportRelations(document->m_Relations, config);
            dbConnection.exportRelationsWays(document->m_Relations);
            dbConnection.exportTags(document->m_SplitWays, config);
            dbConnection.exportWays(document->m_SplitWays, config);


            std::cout << "Creating topology..." << endl;
            dbConnection.createTopology();
        }


        std::cout << "#########################" << endl;

        std::cout << "size of streets: " << document->m_Ways.size() <<    endl;
        std::cout << "size of split ways : " << document->m_SplitWays.size() <<    endl;

        clock_t end = clock();
        double elapsed_secs = double(end - begin) / static_cast<double>(CLOCKS_PER_SEC);

        std::time_t end_t = std::time(NULL);
        std::chrono::steady_clock::time_point end_elapsed = std::chrono::steady_clock::now();

        typedef std::chrono::duration<int,std::milli> millisecs_t ;
        millisecs_t duration = std::chrono::duration_cast<millisecs_t>(end_elapsed - begin_elapsed);

        std::cout << "Execution started at: " << std::ctime(&start_t);
        std::cout << "Execution ended at:   " << std::ctime(&end_t);
        std::cout << "Elapsed time: " << (double)duration.count()/(double)1000 << " Seconds.\n" ;
        std::cout << "User CPU time: -> " << elapsed_secs << " seconds\n";
        
        std::cout << "#########################" << endl;
        //  string n;
        //  getline(cin, n);
        return 0;
    }
    catch (exception &e) {
        std::cout << e.what() << endl;
        return 1;
    } 
    catch (string &e) {
        std::cout << e << endl;
        return 1;
    } 
    catch (...) {
        std::cout << "Terminating" << endl;
        return 1;
    } 
}
