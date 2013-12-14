/*
 * Copyright (C) 2011 Jan Niklas Hasse <jhasse@gmail.com>
 * Copyright (C) Tristan Van Berkom <tristan@upstairslabs.com>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Jan Niklas Hasse <jhasse@gmail.com>
 *   Tristan Van Berkom <tristan@upstairslabs.com>
 */

using GLib;

/***************************************************************
 *                      Debugging Facilities                   *
 ***************************************************************/
[Flags]
public enum DF { 
	COLLECT,
	FILTER
}

static const GLib.DebugKey[] libcwrap_debug_keys = {
	{ "collect", DF.COLLECT },
	{ "filter", DF.FILTER } 
};

private bool libcwrap_debug_initialized = false;
private uint libcwrap_debug_mask = 0;
public delegate void DebugFunc ();

public void libcwrap_note (int domain, DebugFunc debug_func)
{
	if (!libcwrap_debug_initialized) {
		string libcwrap_debug_env = GLib.Environment.get_variable ("LIBCWRAP_DEBUG");

		libcwrap_debug_mask = GLib.parse_debug_string (libcwrap_debug_env, libcwrap_debug_keys);
		libcwrap_debug_initialized = true;
	}

	if ((libcwrap_debug_mask & domain) != 0)
		debug_func ();
}

/***************************************************************
 *                      VersionNumber class                    *
 ***************************************************************/
class VersionNumber : Object
{
	private int major    { get; set; } // x.0.0
	private int minor    { get; set; } // 0.x.0
	private int revision { get; set; } // 0.0.x
	private string originalString;
	
	public VersionNumber(string version)
	{
		originalString = version;
		try
		{
			var regex = new Regex("([[:digit:]]*)\\.([[:digit:]]*)\\.*([[:digit:]]*)");
			var split = regex.split(version);
			assert(split.length > 1); // TODO: Don't use assert, print a nice error message instead
			major = int.parse (split[1]);
			if(split.length > 2)
			{
				minor = int.parse (split[2]);
			}
			else
			{
				minor = 0;
			}
			if(split.length > 3)
			{
				revision = int.parse (split[3]);
			}
			else
			{
				revision = 0;
			}
		}
		catch(GLib.RegexError e)
		{
			stdout.printf("Error compiling regular expression!");
			Posix.exit(-1);
		}
	}
	
	public bool newerThan(VersionNumber other)
	{
		if(major > other.major)
		{
			return true;
		}
		else if(major == other.major)
		{
			if(minor > other.minor)
			{
				return true;
			}
			else if(minor == other.minor)
			{
				if(revision > other.revision)
				{
					return true;
				}
			}
		}
		return false;
	}
	public string getString()
	{
		return originalString;
	}
}

/***************************************************************
 *                         Symbol output                       *
 ***************************************************************/
private void
append_symbols (StringBuilder headerFile, 
				VersionNumber minimumVersion,
				Gee.HashMap<string, VersionNumber> symbolMap,
				Gee.HashSet<string>filterMap,
				bool overrides)
{

	if (overrides)
		headerFile.append("\n/* Symbols introduced in newer glibc versions, which must not be used */\n");
	else
		headerFile.append("\n/* Symbols redirected to earlier glibc versions */\n");

	foreach (var it in symbolMap.keys)
	{
		var version = symbolMap.get (it);
		string versionToUse;

		/* If the symbol only has occurrences older than the minimum required glibc version,
		 * then there is no need to output anything for this symbol
		 */
		if (filterMap.contains (it))
			continue;

		if (overrides && version.newerThan (minimumVersion))
			versionToUse = "DONT_USE_THIS_VERSION_%s".printf (version.getString());
		else if (!overrides && !version.newerThan (minimumVersion))
			versionToUse = version.getString ();
		else
			continue;

		headerFile.append("__asm__(\".symver %s, %s@GLIBC_%s\");\n".printf(it, it, versionToUse));
	}
}

/***************************************************************
 *                             Main                            *
 ***************************************************************/
int main(string[] args)
{
	try
	{
		if(args.length != 4)
		{
			stdout.printf("Usage: buildlist <header file to create> <minimum glibc version> <system library directory>\n");
			return 1;
		}

		var minimumVersion = new VersionNumber(args[2]);
		var filename = args[1];
		var syslibs = args[3];

		stdout.printf ("Generating %s (glibc %s) from libs at '%s' .", filename, minimumVersion.getString(), syslibs);

		var headerFile = new StringBuilder ();
		headerFile.append ("/* glibc bindings for target ABI version glibc " + minimumVersion.getString() + " */\n");
		stdout.flush();

		string output, errorOutput;
		int returnCode;
		var libPath = File.new_for_path (syslibs);
		var enumerator = libPath.enumerate_children (FileAttribute.STANDARD_NAME, 0, null);
		FileInfo fileinfo;
		var counter = 0;

		/* This map will contain every symbol with new version as close to the minimum version as possible
		 * including symbols which have a higher version than the minimum
		 */
		var symbolMap = new Gee.HashMap<string, VersionNumber>((Gee.HashDataFunc<string>)GLib.str_hash,
															   (Gee.EqualDataFunc<string>)GLib.str_equal);

		/* This map will contain only symbols for which a version newer than the minimum was not found,
		 * these symbols are safe to exclude from the output
		 */
		var symbolsOlderThanMinimum = new Gee.HashSet<string>((Gee.HashDataFunc<string>)GLib.str_hash,
															  (Gee.EqualDataFunc<string>)GLib.str_equal);

		while ((fileinfo = enumerator.next_file(null)) != null)
		{
			if(++counter % 50 == 0)
			{
				stdout.printf(".");
				stdout.flush();
			}

			Process.spawn_command_line_sync("objdump -T " + syslibs + "/" + fileinfo.get_name(), 
											out output, out errorOutput, out returnCode);

			if (returnCode != 0)
				continue;

			foreach (var line in output.split("\n"))
			{
				var regex = new Regex("(.*)(GLIBC_)([0-9]+\\.([0-9]+\\.)*[0-9]+)(\\)?)([ ]*)(.+)");

				if (regex.match (line) && !("PRIVATE" in line))
				{
					var version = new VersionNumber (regex.split (line)[3]);
					var symbolName = regex.split (line)[7];
					var versionInMap = symbolMap.get (symbolName);

					/* Some versioning symbols exist in the objdump output, let's skip those */
					if (symbolName.has_prefix ("GLIBC"))
						continue;

					libcwrap_note (DF.COLLECT, () =>
							 stdout.printf ("Selected symbol '%s' version '%s' from objdump line %s\n", 
											symbolName, version.getString(), line));

					if (versionInMap == null)
					{
						symbolMap.set(symbolName, version);

						/* First occurance of the symbol, if it's older
						 * than the minimum required, put it in that table also
						 */
						if (minimumVersion.newerThan (version))
						{
							symbolsOlderThanMinimum.add(symbolName);

							libcwrap_note (DF.FILTER, () =>
									 stdout.printf ("Adding symbol '%s %s' to the filter\n", 
													symbolName, version.getString()));
						}
					}
					else
					{
						/* We want the newest possible version of a symbol which is older than the
						 * minimum glibc version specified (or the only version of the symbol if
						 * it's newer than the minimum version)
						 */
						if(version.newerThan (versionInMap) && minimumVersion.newerThan (version))
							symbolMap.set(symbolName, version);

						/* While trucking along through the huge symbol list, remove symbols from
						 * the 'safe to exclude' if there is a version found which is newer
						 * than the minimum requirement
						 */
						if (version.newerThan (minimumVersion))
						{
							symbolsOlderThanMinimum.remove(symbolName);

							libcwrap_note (DF.FILTER, () =>
									 stdout.printf ("Removing symbol '%s %s' from the filter\n", 
													symbolName, version.getString()));
						}
					}
				}
				else
				{
					libcwrap_note (DF.COLLECT, () => stdout.printf ("Rejected objdump line %s\n", line));
				}
			}
		}
		
		headerFile.append ("#if !defined (__LIBC_CUSTOM_BINDINGS_H__)\n");
		headerFile.append ("\n");
		headerFile.append ("#  if !defined (__OBJC__) && !defined (__ASSEMBLER__)\n");
		headerFile.append ("#    if defined (__cplusplus)\n");
		headerFile.append ("extern \"C\" {\n");
		headerFile.append ("#    endif\n");

		/* For prettier output, let's output the redirected symbols first, and
		 * then output the ones which must not be used (new in glibc > minimumVersion).
		 */
		append_symbols (headerFile, minimumVersion,
						symbolMap, symbolsOlderThanMinimum,
						false);

		append_symbols (headerFile, minimumVersion,
						symbolMap, symbolsOlderThanMinimum,
						true);

		headerFile.append ("\n");
		headerFile.append ("#    if defined (__cplusplus)\n");
		headerFile.append ("}\n");
		headerFile.append ("#    endif\n");
		headerFile.append ("#  endif /* !defined (__OBJC__) && !defined (__ASSEMBLER__) */\n");
		headerFile.append ("#endif\n");

		FileUtils.set_contents(filename, headerFile.str);
	}
	catch(Error e)
	{
		warning("%s", e.message);
		return 1;
	}
	stdout.printf(" OK\n");
	return 0;
}
