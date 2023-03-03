package org.diasurgical.devilutionx;

import android.content.Context;
import android.os.Build;
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Objects;

public class ExternalFilesManager {
	private String externalFilesDirectory;

	public ExternalFilesManager(Context context) {
		externalFilesDirectory = chooseExternalFilesDirectory(context);
	}

	public String getExternalFilesDirectory() {
		return externalFilesDirectory;
	}

	public boolean hasFile(String fileName) {
		File file = getFile(fileName);
		return file.exists();
	}

	public File getFile(String fileName) {
		return new File(externalFilesDirectory + "/" + fileName);
	}

	public void migrateFile(File file) {
		File newPath = new File(externalFilesDirectory + "/" + file.getName());

		if (newPath.exists()) {
			if (file.canWrite()) {
				//noinspection ResultOfMethodCallIgnored
				file.delete();
			}
			return;
		}
		if (!file.renameTo(newPath)) {
			if (copyFile(file, newPath) && file.canWrite()) {
				//noinspection ResultOfMethodCallIgnored
				file.delete();
			}
		}
	}

	private String chooseExternalFilesDirectory(Context context) {
		if (Build.VERSION.SDK_INT >= 19) {
			File[] externalDirs = context.getExternalFilesDirs(null);

			for (int i = 0; i < externalDirs.length; i++) {
				File dir = externalDirs[i];
				File[] iniFiles = dir.listFiles((dir1, name) -> name.equals("diablo.ini"));
				if (iniFiles.length > 0)
					return dir.getAbsolutePath();
			}

			for (int i = 0; i < externalDirs.length; i++) {
				File dir = externalDirs[i];
				if (dir.listFiles().length > 0)
					return dir.getAbsolutePath();
			}
		}

		return context.getExternalFilesDir(null).getAbsolutePath();
	}

	private boolean copyFile(File src, File dst) {
		try {
			InputStream in = new FileInputStream(src);
			try {
				OutputStream out = new FileOutputStream(dst);
				try {
					// Transfer bytes from in to out
					byte[] buf = new byte[1024];
					int len;
					while ((len = in.read(buf)) > 0) {
						out.write(buf, 0, len);
					}
				} finally {
					out.close();
				}
			} finally {
				in.close();
			}
		} catch (IOException exception) {
			Log.e("copyFile", Objects.requireNonNull(exception.getMessage()));
			if (dst.exists()) {
				//noinspection ResultOfMethodCallIgnored
				dst.delete();
			}
			return false;
		}

		return true;
	}
}
