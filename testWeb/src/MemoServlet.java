import Thymeleaf.viewBaseServlet;
import org.thymeleaf.context.WebContext;

import javax.servlet.ServletException;
import javax.servlet.annotation.WebServlet;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

@WebServlet("/upload")
public class MemoServlet extends viewBaseServlet {
    @Override
    protected void doPost(HttpServletRequest req, HttpServletResponse resp) throws ServletException, IOException {
        HttpSession session = req.getSession();
        System.out.println("/upload");
        System.out.println("当前Session ID:"+session.getId());
        System.out.println("当前备忘录列表："+session.getAttribute("allContents"));
        System.out.println("请求Cookie:"+req.getHeader("Cookie"));
        System.out.println("*********************************************");
        req.setCharacterEncoding("UTF-8");
        resp.setCharacterEncoding("UTF-8");
        resp.setContentType("text/html;charset=UTF-8");
        String content = req.getParameter("note");
        System.out.println(content);

        List<String> allContents = (List<String>) session.getAttribute("allContents");

        if (allContents == null) {
            allContents = new ArrayList<>();
        }

        allContents.add(content);
        session.setAttribute("allContents", allContents);
        // req.setAttribute("allContents", allContents);
        // ✅ 使用 Thymeleaf 渲染模板为 HTML 字符串
        WebContext context = new WebContext(req, resp, getServletContext());
        context.setVariable("allContents", allContents);

        String html = this.getTemplateEngine().process("index", context);

        // ✅ 禁用 chunked：转为字节流 + 设置 content-length
        byte[] htmlBytes = html.getBytes(StandardCharsets.UTF_8);
        resp.setContentLength(htmlBytes.length);  // 💡 关键：必须明确告知长度

        resp.getOutputStream().write(htmlBytes);
        resp.getOutputStream().flush();
    }
}
