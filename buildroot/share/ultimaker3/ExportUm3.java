// Headless static analysis export; does not execute target code.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.nio.file.*;
import java.io.*;

public class ExportUm3 extends GhidraScript {
  public void run() throws Exception {
    Path output = Paths.get(getScriptArgs()[0]);
    Files.createDirectories(output);
    DecompInterface decompiler = new DecompInterface();
    decompiler.openProgram(currentProgram);
    int count = 0;
    boolean avr = currentProgram.getLanguage().getProcessor().toString().equals("AVR8");
    try (PrintWriter index = new PrintWriter(Files.newBufferedWriter(output.resolve("functions.tsv")));
         PrintWriter symbols = new PrintWriter(Files.newBufferedWriter(output.resolve("symbols.tsv")))) {
      SymbolIterator allSymbols = currentProgram.getSymbolTable().getAllSymbols(true);
      while (allSymbols.hasNext()) {
        Symbol s = allSymbols.next();
        symbols.println(s.getAddress() + "\t" + s.getName(true) + "\t" + s.getSymbolType());
      }
      FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
      while (functions.hasNext() && !monitor.isCancelled()) {
        Function f = functions.next();
        String name = f.getName(true);
        if (!avr && !name.matches(".*(Temperature|temperature|heater|Heater|PowerBudget|Board|cap|Cap|Sample|probe|Probe|plan_|quickStop|st_|stepper|Stepper|Stop|stop|fan|Fan|analog2|tp_|setup|watchdog|check_serial|GainCompensator).*")) continue;
        DecompileResults result = decompiler.decompileFunction(f, 90, monitor);
        String address = f.getEntryPoint().toString();
        String filename = address.replace(':', '_') + ".c";
        index.println(address + "\t" + name + "\t" + f.getBody().getNumAddresses() + "\t" + filename + "\t" + result.decompileCompleted());
        index.flush();
        if (result.decompileCompleted()) {
          String code = "/* Ghidra decompilation of shipped UM3 binary; not original source. AVR code addresses are word-addressed.\n * Function: " + name + " @ " + address + "\n */\n" + result.getDecompiledFunction().getC();
          Files.writeString(output.resolve(filename), code);
          count++;
          println("Exported " + address + " " + name);
        } else println("FAILED " + address + " " + name + " " + result.getErrorMessage());
      }
    } finally { decompiler.dispose(); }
    println("RECOVERY_EXPORT_COUNT=" + count);
  }
}
