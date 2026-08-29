package xyz.waozi.uku;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import java.io.File;
import java.io.FileNotFoundException;

public class ShareProvider extends ContentProvider {
    private static final String AUTHORITY = "xyz.waozi.uku.shareprovider";

    static File shareDir(Context context) {
        return new File(context.getCacheDir(), "share");
    }

    static Uri uriForFile(File file) {
        return new Uri.Builder()
                .scheme("content")
                .authority(AUTHORITY)
                .appendPath(file.getName())
                .build();
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public String getType(Uri uri) {
        String name = safeName(uri);
        if (name.endsWith(".png")) return "image/png";
        if (name.endsWith(".txt")) return "text/plain";
        return "application/octet-stream";
    }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        File file = resolve(uri);
        if (!file.isFile()) throw new FileNotFoundException(uri.toString());
        return ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY);
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection,
                        String[] selectionArgs, String sortOrder) {
        File file;
        MatrixCursor cursor = new MatrixCursor(new String[] {
                OpenableColumns.DISPLAY_NAME,
                OpenableColumns.SIZE
        });
        try {
            file = resolve(uri);
            cursor.addRow(new Object[] { file.getName(), file.length() });
        } catch (FileNotFoundException ignored) {
        }
        return cursor;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        return 0;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection,
                      String[] selectionArgs) {
        return 0;
    }

    private File resolve(Uri uri) throws FileNotFoundException {
        Context context = getContext();
        String name = safeName(uri);
        if (context == null || name.isEmpty()) {
            throw new FileNotFoundException(uri.toString());
        }
        return new File(shareDir(context), name);
    }

    private static String safeName(Uri uri) {
        String name = uri != null ? uri.getLastPathSegment() : "";
        if (name == null) return "";
        if (name.contains("/") || name.contains("\\") || name.equals(".") || name.equals("..")) {
            return "";
        }
        return name;
    }
}
