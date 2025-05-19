import Thymeleaf.viewBaseServlet;

import javax.servlet.ServletException;
import javax.servlet.annotation.WebServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;
import java.io.IOException;
import java.util.List;

@WebServlet("/delete")
public class deleteServlet extends viewBaseServlet {
    @Override
    protected void doDelete(HttpServletRequest req, HttpServletResponse resp) throws ServletException, IOException {
        HttpSession session = req.getSession();
        req.setCharacterEncoding("utf-8");
        resp.setCharacterEncoding("utf-8");
        resp.setContentType("text/html;charset=UTF-8");
        resp.setHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        resp.setHeader("Pragma", "no-cache");
        resp.setHeader("Expires", "0");
        resp.setHeader("Access-Control-Allow-Origin", "http://212.129.223.4:8080");
        resp.setHeader("Access-Control-Allow-Credentials", "true");
        resp.setHeader("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");
        resp.setHeader("Access-Control-Allow-Headers", "Content-Type");



        System.out.println("/delete");
        System.out.println("当前Session ID:"+session.getId());
        System.out.println("当前备忘录列表："+session.getAttribute("allContents"));
        System.out.println("请求Cookie:"+req.getHeader("Cookie"));
        System.out.println("*********************************************");

        List<String> allContents = (List<String>) session.getAttribute("allContents");
        String note = req.getParameter("note");
        if (allContents != null && note != null) {
            allContents.remove(note);
            session.setAttribute("allContents", allContents);
            resp.getWriter().write("ok");
        } else {
            resp.setStatus(400);
            resp.getWriter().write("删除失败");
        }
    }
}
