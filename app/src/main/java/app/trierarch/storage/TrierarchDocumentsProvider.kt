package app.trierarch.storage

import android.database.Cursor
import android.database.MatrixCursor
import android.os.CancellationSignal
import android.os.ParcelFileDescriptor
import android.provider.DocumentsContract.Document
import android.provider.DocumentsContract.Root
import android.provider.DocumentsProvider
import android.system.Os
import android.system.OsConstants
import android.webkit.MimeTypeMap
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.IOException

/**
 * Exposes Trierarch's complete app-private data directory through Android's
 * standard Storage Access Framework. No file-manager-specific protocol is used.
 */
class TrierarchDocumentsProvider : DocumentsProvider() {
    override fun onCreate(): Boolean {
        dataRoot = requireNotNull(context).applicationInfo.dataDir.let(::File).canonicalFile
        return true
    }

    override fun queryRoots(projection: Array<String>?): Cursor = MatrixCursor(
        projection ?: ROOT_COLUMNS,
    ).apply {
        newRow()
            .add(Root.COLUMN_ROOT_ID, ROOT_ID)
            .add(Root.COLUMN_DOCUMENT_ID, ROOT_ID)
            .add(Root.COLUMN_TITLE, "Trierarch internal")
            .add(Root.COLUMN_SUMMARY, "Full Trierarch app-private storage")
            .add(Root.COLUMN_FLAGS, Root.FLAG_LOCAL_ONLY or Root.FLAG_SUPPORTS_CREATE)
            .add(Root.COLUMN_MIME_TYPES, "*/*")
            .add(Root.COLUMN_AVAILABLE_BYTES, dataRoot.usableSpace)
    }

    override fun queryDocument(documentId: String, projection: Array<String>?): Cursor = MatrixCursor(
        projection ?: DOCUMENT_COLUMNS,
    ).apply {
        includeDocument(this, resolveDocument(documentId), documentId)
    }

    override fun queryChildDocuments(
        parentDocumentId: String,
        projection: Array<String>?,
        sortOrder: String?,
    ): Cursor = MatrixCursor(projection ?: DOCUMENT_COLUMNS).apply {
        val parent = resolveDocument(parentDocumentId)
        require(parent.isDirectory) { "$parentDocumentId is not a directory" }
        parent.listFiles()
            ?.sortedWith(compareBy<File>({ !it.isDirectory }, { it.name.lowercase() }))
            ?.forEach { child ->
                val childId = documentIdFor(child)
                if (isExposed(child)) includeDocument(this, child, childId)
            }
    }

    override fun openDocument(
        documentId: String,
        mode: String,
        signal: CancellationSignal?,
    ): ParcelFileDescriptor = ParcelFileDescriptor.open(
        resolveDocument(documentId),
        ParcelFileDescriptor.parseMode(mode),
    )

    override fun createDocument(
        parentDocumentId: String,
        mimeType: String,
        displayName: String,
    ): String {
        val parent = resolveDocument(parentDocumentId)
        require(parent.isDirectory) { "$parentDocumentId is not a directory" }
        val child = File(parent, validateName(displayName))
        require(!child.exists()) { "${child.name} already exists" }
        if (mimeType == Document.MIME_TYPE_DIR) {
            check(child.mkdir()) { "Unable to create directory ${child.name}" }
        } else {
            FileOutputStream(child).close()
        }
        return documentIdFor(child)
    }

    override fun deleteDocument(documentId: String) {
        require(documentId != ROOT_ID) { "The Trierarch storage root cannot be deleted" }
        deleteRecursively(resolveDocument(documentId))
    }

    override fun renameDocument(documentId: String, displayName: String): String {
        require(documentId != ROOT_ID) { "The Trierarch storage root cannot be renamed" }
        val source = resolveDocument(documentId)
        val destination = File(requireNotNull(source.parentFile), validateName(displayName))
        require(!destination.exists()) { "${destination.name} already exists" }
        check(source.renameTo(destination)) { "Unable to rename ${source.name}" }
        return documentIdFor(destination)
    }

    override fun moveDocument(
        sourceDocumentId: String,
        sourceParentDocumentId: String,
        targetParentDocumentId: String,
    ): String {
        require(sourceDocumentId != ROOT_ID) { "The Trierarch storage root cannot be moved" }
        val source = resolveDocument(sourceDocumentId)
        val sourceParent = resolveDocument(sourceParentDocumentId)
        require(source.parentFile?.canonicalFile == sourceParent) { "Source parent does not match" }
        val targetParent = resolveDocument(targetParentDocumentId)
        require(targetParent.isDirectory) { "$targetParentDocumentId is not a directory" }
        val destination = File(targetParent, source.name)
        require(!destination.exists()) { "${destination.name} already exists" }
        check(source.renameTo(destination)) { "Unable to move ${source.name}" }
        return documentIdFor(destination)
    }

    override fun copyDocument(
        sourceDocumentId: String,
        targetParentDocumentId: String,
    ): String {
        val source = resolveDocument(sourceDocumentId)
        val targetParent = resolveDocument(targetParentDocumentId)
        require(targetParent.isDirectory) { "$targetParentDocumentId is not a directory" }
        val destination = File(targetParent, source.name)
        require(!destination.exists()) { "${destination.name} already exists" }
        copyRecursively(source, destination)
        return documentIdFor(destination)
    }

    override fun isChildDocument(parentDocumentId: String, documentId: String): Boolean =
        runCatching {
            val parent = resolveDocument(parentDocumentId)
            val document = resolveDocument(documentId)
            document != parent && document.canonicalPath.startsWith(parent.canonicalPath + File.separator)
        }.getOrDefault(false)

    private fun includeDocument(cursor: MatrixCursor, file: File, documentId: String) {
        val isRoot = documentId == ROOT_ID
        val directory = file.isDirectory
        var flags = 0
        if (!isRoot && file.canWrite()) {
            flags = flags or Document.FLAG_SUPPORTS_DELETE or Document.FLAG_SUPPORTS_RENAME or
                Document.FLAG_SUPPORTS_MOVE or Document.FLAG_SUPPORTS_COPY
        }
        if (file.canWrite() && !directory) flags = flags or Document.FLAG_SUPPORTS_WRITE
        if (file.canWrite() && directory) flags = flags or Document.FLAG_DIR_SUPPORTS_CREATE

        cursor.newRow()
            .add(Document.COLUMN_DOCUMENT_ID, documentId)
            .add(Document.COLUMN_DISPLAY_NAME, if (isRoot) "Trierarch internal" else file.name)
            .add(Document.COLUMN_MIME_TYPE, if (directory) Document.MIME_TYPE_DIR else mimeTypeFor(file))
            .add(Document.COLUMN_FLAGS, flags)
            .add(Document.COLUMN_SIZE, if (directory) null else file.length())
            .add(Document.COLUMN_LAST_MODIFIED, file.lastModified())
    }

    private fun resolveDocument(documentId: String): File {
        val relative = when {
            documentId == ROOT_ID -> ""
            documentId.startsWith("$ROOT_ID/") -> documentId.removePrefix("$ROOT_ID/")
            else -> throw IllegalArgumentException("Unknown document ID: $documentId")
        }
        val path = if (relative.isEmpty()) dataRoot else File(dataRoot, relative)
        relative.split('/').filter(String::isNotEmpty).forEach { segment ->
            require(segment != "." && segment != ".." && !segment.contains(File.separatorChar)) {
                "Unsafe document path"
            }
        }
        require(isExposed(path)) { "Document is outside Trierarch storage" }
        return path
    }

    private fun documentIdFor(file: File): String {
        val path = file.absoluteFile.toPath().normalize()
        val rootPath = dataRoot.toPath()
        require(path.startsWith(rootPath)) { "Document is outside Trierarch storage" }
        val relative = rootPath.relativize(path).toString().replace(File.separatorChar, '/')
        return if (relative.isEmpty()) ROOT_ID else "$ROOT_ID/$relative"
    }

    private fun isExposed(file: File): Boolean = runCatching {
        val canonical = file.canonicalFile
        canonical == dataRoot || canonical.path.startsWith(dataRoot.path + File.separator)
    }.getOrDefault(false)

    private fun validateName(name: String): String {
        require(name.isNotBlank() && name != "." && name != ".." && '/' !in name && File.separatorChar !in name) {
            "Unsafe document name"
        }
        return name
    }

    private fun deleteRecursively(file: File) {
        if (file.isDirectory && !isSymbolicLink(file)) {
            file.listFiles()?.forEach(::deleteRecursively)
        }
        check(file.delete()) { "Unable to delete ${file.name}" }
    }

    private fun copyRecursively(source: File, destination: File) {
        if (isSymbolicLink(source)) {
            Os.symlink(Os.readlink(source.absolutePath), destination.absolutePath)
            return
        }
        if (source.isDirectory) {
            check(destination.mkdir()) { "Unable to create directory ${destination.name}" }
            source.listFiles()?.forEach { child -> copyRecursively(child, File(destination, child.name)) }
            return
        }
        FileInputStream(source).use { input ->
            FileOutputStream(destination).use { output -> input.copyTo(output) }
        }
    }

    private fun isSymbolicLink(file: File): Boolean = try {
        (Os.lstat(file.absolutePath).st_mode and OsConstants.S_IFMT) == OsConstants.S_IFLNK
    } catch (_: Exception) {
        false
    }

    private fun mimeTypeFor(file: File): String = MimeTypeMap.getSingleton()
        .getMimeTypeFromExtension(file.extension.lowercase())
        ?: "application/octet-stream"

    private lateinit var dataRoot: File

    companion object {
        private const val ROOT_ID = "root"
        private val ROOT_COLUMNS = arrayOf(
            Root.COLUMN_ROOT_ID,
            Root.COLUMN_DOCUMENT_ID,
            Root.COLUMN_TITLE,
            Root.COLUMN_SUMMARY,
            Root.COLUMN_FLAGS,
            Root.COLUMN_MIME_TYPES,
            Root.COLUMN_AVAILABLE_BYTES,
        )
        private val DOCUMENT_COLUMNS = arrayOf(
            Document.COLUMN_DOCUMENT_ID,
            Document.COLUMN_DISPLAY_NAME,
            Document.COLUMN_MIME_TYPE,
            Document.COLUMN_FLAGS,
            Document.COLUMN_SIZE,
            Document.COLUMN_LAST_MODIFIED,
        )
    }
}
