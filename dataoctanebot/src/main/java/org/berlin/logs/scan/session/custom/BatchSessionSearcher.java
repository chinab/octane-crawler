/**
 * Copyright (c) 2006-2011 Berlin Brown.  All Rights Reserved
 *
 * http://www.opensource.org/licenses/bsd-license.php
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * * Neither the name of the Botnode.com (Berlin Brown) nor
 * the names of its contributors may be used to endorse or promote
 * products derived from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Date: 8/15/2011
 *  
 * Description: LogFile Searcher.  Search log file and build statistics from the scan.
 * 
 * Contact: Berlin Brown <berlin dot brown at gmail.com>
 */
package org.berlin.logs.scan.session.custom;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.PrintWriter;
import java.util.LinkedHashMap;
import java.util.Map;

import org.berlin.logs.scan.GlobalConfiguration;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class BatchSessionSearcher {

  private static final Logger LOGGER = LoggerFactory.getLogger(BatchSessionSearcher.class);
  
  private final GlobalConfiguration globalConf;
  private int statsTotalSearchFound = 0;

  boolean writeOutputFile = true;
  String outputFilename = "xxx-output.log";
  private PrintWriter printWriter;

  String regexSessionPattern = ".*\\d (\\[(WebContainer) : (\\d+)\\] \\[(\\S{23}) \\- (\\w{5})\\] (\\w*)) .*";
  boolean useXMLFormat = true;

  int smallerDefaultReadBufferSize = 10 * 1024 * 1024;
  boolean readMoreLargeFile = true;

  private final Map<String, SessionInfo> sessionDatabase = new LinkedHashMap<String, SessionInfo>();

  /**
   * Constructor for LogSearch.
   * 
   * @param globalConf
   */
  public BatchSessionSearcher(final GlobalConfiguration globalConf) {
    this.globalConf = globalConf;
  }

  /**
   * Search for term in log files.
   * 
   * @return
   */
  public BatchSessionSearcher search() {

    final long tstart = System.currentTimeMillis();
    this.openOutputFile();
    final String dir = this.globalConf.getFileCopyLocalTargetDir();
    final String searchTerm = this.globalConf.getUserSearchTerm();
    if (searchTerm == null || searchTerm.length() == 0) {
      LOGGER.info("Invalid search term");
      return this;
    }
    LOGGER.info("Searching directory : " + dir);
    LOGGER.info(">> Searching for term '" + searchTerm + "'");
    final File fd = new File(dir);
    if (!fd.isDirectory()) {
      LOGGER.info("Target path is not a directory, exiting");
      return this;
    }
    int totalAllFiles = 0;
    int lastIdValue = 0;

    if (globalConf.isUseXMLPropertyFormat()) {
      this.printWriter.println("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>");
      this.printWriter.println("<!DOCTYPE properties SYSTEM \"http://java.sun.com/dtd/properties.dtd\">");
      this.printWriter.println("<properties>");
    }
    for (final File f : fd.listFiles()) {
      if (f.isDirectory()) {
        continue;
      }
      final BasicSesssionCollector s = new BasicSesssionCollector(globalConf, sessionDatabase);
      s.writeOutputFile = this.writeOutputFile;
      s.outputFilename = this.outputFilename;
      s.setPrintWriter(printWriter);
      s.lastIdValue = lastIdValue;
      s.search(regexSessionPattern, f);
      lastIdValue = s.id;
      totalAllFiles += s.getTermFoundTotal();
    } // End of the For //
    // Write the full session database to file //
    final BasicSesssionCollector s = new BasicSesssionCollector(globalConf, sessionDatabase);
    s.writeOutputFile = this.writeOutputFile;
    s.outputFilename = this.outputFilename;
    s.setPrintWriter(printWriter);
    s.writeSessionDatabase();
    if (globalConf.isUseXMLPropertyFormat()) {
      this.printWriter.println("</properties>");
    }
    this.statsTotalSearchFound = totalAllFiles;
    this.close();
    final long tdiff = System.currentTimeMillis() - tstart;
    LOGGER.info(">> Found term '" + searchTerm + "' total of = " + this.statsTotalSearchFound + " times in files");
    LOGGER.info(">> Found term in " + tdiff + " ms , " + (tdiff / 1000.0) + " seconds");

    final double mb = 1024.0 * 1024;
    final double free = Runtime.getRuntime().freeMemory() / mb;
    final double total = Runtime.getRuntime().totalMemory() / mb;
    final double max = Runtime.getRuntime().maxMemory() / mb;
    final String fmt = String.format("Memory after operation [ freeMemory=%.2fM total=%.2fM maxMemory=%.2fM ]", free, total, max);
    LOGGER.info(fmt);
    return this;
  }

  /**
   * Open output file.
   */
  public void openOutputFile() {

    if (!this.writeOutputFile) {
      return;
    }
    if (this.outputFilename == null || this.outputFilename.length() == 0) {
      return;
    }
    BufferedOutputStream bos;
    try {
      bos = new BufferedOutputStream(new FileOutputStream(this.outputFilename));
      this.printWriter = new PrintWriter(bos);
    } catch (final FileNotFoundException e) {
      e.printStackTrace();
      throw new IllegalStateException("Could not open output file");
    }
  }

  /**
   * Close the open output file.
   */
  public void close() {
    if (!this.writeOutputFile) {
      return;
    }
    if (this.outputFilename == null || this.outputFilename.length() == 0) {
      return;
    }
    if (this.printWriter != null) {
      this.printWriter.close();

      final File f = new File(this.outputFilename);
      LOGGER.info("SessionSearch : Closing output file, see file for results : " + f.getAbsolutePath() + " parentDir=" + f.getParent());
    }
  }

  /**
   * @return the statsTotalSearchFound
   */
  public int getStatsTotalSearchFound() {
    return statsTotalSearchFound;
  }

  /**
   * @param writeOutputFile
   *          the writeOutputFile to set
   */
  public void setWriteOutputFile(boolean writeOutputFile) {
    this.writeOutputFile = writeOutputFile;
  }

  /**
   * @param outputFilename
   *          the outputFilename to set
   */
  public void setOutputFilename(String outputFilename) {
    this.outputFilename = outputFilename;
  }

  public boolean isUseXMLFormat() {
    return useXMLFormat;
  }

  public void setUseXMLFormat(boolean useXMLFormat) {
    this.useXMLFormat = useXMLFormat;
  }

} // End of Class //
