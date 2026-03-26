export async function onRequestGet(context) {
  const { searchParams } = new URL(context.request.url);
  const query = searchParams.get("q");
  const db = context.env.DB;

  const { results } = await db.prepare(
    "SELECT * FROM hazards WHERE location_name LIKE ? OR type LIKE ?"
  )
  .bind(`%${query}%`, `%${query}%`)
  .all();

  return new Response(JSON.stringify(results), {
    headers: { "Content-Type": "application/json" },
  });
}
